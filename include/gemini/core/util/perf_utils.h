#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <utility>
#include <functional>

namespace gemini::perf
{

// =============================================================
// Build-time enable/disable switch
// =============================================================
#ifndef CHEETAH_ENABLE_PERF
#define CHEETAH_ENABLE_PERF 1
#endif

    // =============================================================
    // Clock helpers
    // =============================================================
    using Clock = std::chrono::high_resolution_clock;

    inline double ms_since(const Clock::time_point &start)
    {
#if CHEETAH_ENABLE_PERF
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
#else
        (void)start;
        return 0.0;
#endif
    }

    // =============================================================
    // Optional thread-safe printing
    // =============================================================
    inline std::mutex &cout_mutex()
    {
        static std::mutex m;
        return m;
    }

    inline void print_line(const std::string &s)
    {
#if CHEETAH_ENABLE_PERF
        std::lock_guard<std::mutex> lk(cout_mutex());
        std::cout << s;
#else
        (void)s;
#endif
    }

    // =============================================================
    // IO BYTE MEASUREMENT
    // =============================================================
    struct IOBytesDelta
    {
        uint64_t begin = 0;
        uint64_t end = 0;

        uint64_t bytes() const { return end - begin; }
        double mib() const { return double(bytes()) / 1024.0 / 1024.0; }
    };

    // ---------------------------------------------
    // 1) Pointer-based counter scope (single counter)
    // ---------------------------------------------
    class IOScope
    {
    public:
        IOScope() = default;

        IOScope(uint64_t *counter, std::string label = {})
            : counter_(counter), label_(std::move(label))
        {
#if CHEETAH_ENABLE_PERF
            if (counter_)
                begin_ = *counter_;
#endif
        }

        IOBytesDelta finish()
        {
#if CHEETAH_ENABLE_PERF
            if (finished_)
                return last_;
            last_ = IOBytesDelta{begin_, counter_ ? *counter_ : begin_};
            finished_ = true;
            return last_;
#else
            finished_ = true;
            return IOBytesDelta{};
#endif
        }

        const std::string &label() const { return label_; }

        ~IOScope()
        {
#if CHEETAH_ENABLE_PERF
            if (!finished_ && counter_ && !label_.empty())
            {
                IOBytesDelta d{begin_, *counter_};
                std::lock_guard<std::mutex> lk(cout_mutex());
                std::cerr << "[io] " << label_ << ": "
                          << d.bytes() << " B (" << d.mib() << " MiB)\n";
            }
#endif
        }

    private:
        uint64_t *counter_ = nullptr;
        std::string label_;
        uint64_t begin_ = 0;
        bool finished_ = false;
        IOBytesDelta last_{};
    };

    // ----------------------------------------------------
    // 2) Reader-based scope (aggregated / computed counter)
    //    e.g. sum of ctx_[i].io->counter across threads
    // ----------------------------------------------------
    class MultiIOScope
    {
    public:
        using Reader = std::function<uint64_t()>;

        MultiIOScope() = default;

        MultiIOScope(Reader reader, std::string label = {})
            : reader_(std::move(reader)), label_(std::move(label))
        {
#if CHEETAH_ENABLE_PERF
            if (reader_)
                begin_ = reader_();
#endif
        }

        IOBytesDelta finish()
        {
#if CHEETAH_ENABLE_PERF
            if (finished_)
                return last_;
            last_ = IOBytesDelta{begin_, reader_ ? reader_() : begin_};
            finished_ = true;
            return last_;
#else
            finished_ = true;
            return IOBytesDelta{};
#endif
        }

        ~MultiIOScope()
        {
#if CHEETAH_ENABLE_PERF
            if (!finished_ && reader_ && !label_.empty())
            {
                IOBytesDelta d{begin_, reader_()};
                std::lock_guard<std::mutex> lk(cout_mutex());
                std::cerr << "[io] " << label_ << ": "
                          << d.bytes() << " B (" << d.mib() << " MiB)\n";
            }
#endif
        }

    private:
        Reader reader_;
        std::string label_;
        uint64_t begin_ = 0;
        bool finished_ = false;
        IOBytesDelta last_{};
    };

    // =============================================================
    // SCOPED TIMER
    // =============================================================
    struct ScopedTimer
    {
        const char *label = nullptr;
        Clock::time_point t0{};

        explicit ScopedTimer(const char *lbl) : label(lbl)
        {
#if CHEETAH_ENABLE_PERF
            t0 = Clock::now();
#else
            (void)lbl;
#endif
        }

        ~ScopedTimer()
        {
#if CHEETAH_ENABLE_PERF
            std::lock_guard<std::mutex> lk(cout_mutex());
            std::cout << "  [time] " << std::left << std::setw(28)
                      << (label ? label : "") << ms_since(t0) << " ms\n";
#endif
        }
    };

    // =============================================================
    // STAGE TIMER (header + TOTAL). No IO.
    // =============================================================
    struct StageTimer
    {
        std::string prefix;
        const char *name = nullptr;
        Clock::time_point t0{};

        StageTimer(const char *n, std::string pref = {})
            : prefix(std::move(pref)), name(n)
        {
#if CHEETAH_ENABLE_PERF
            t0 = Clock::now();
            std::lock_guard<std::mutex> lk(cout_mutex());
            std::cout << "\n"
                      << prefix << (prefix.empty() ? "" : " ")
                      << (name ? name : "") << "\n";
#else
            (void)n;
#endif
        }

        void done() const
        {
#if CHEETAH_ENABLE_PERF
            std::lock_guard<std::mutex> lk(cout_mutex());
            std::cout << "  [time] " << std::left << std::setw(28)
                      << "TOTAL" << ms_since(t0) << " ms\n";
#endif
        }
    };

} // namespace gemini::perf
