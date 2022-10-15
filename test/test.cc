#include "fast_logger.h"
#include "stra_logger.h"
#include "tscns.h"
#include <sys/time.h>

inline static uint64_t get_current_nano_sec()
{
    timespec ts;
    ::clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

int main()
{
    TSCNS tscns;
    stra_logger *log = new stra_logger(tscns);
    log->set_log_file("../test.log");
    STRA_LOG(log, DEBUG, "%d %d %s %lf", 10, 111, "test", 1.020215);
    log->poll();
    STRA_LOG(log, DEBUG, "%d %d %s %lf", 10, 111, "test2", 1.020215);
    log->poll();

    stra_logger *log_1 = new stra_logger(tscns);
    log_1->set_log_file("../test1.log");
    STRA_LOG(log_1, DEBUG, "%d %d %s %.1lf", 10, 111, "test", 1.020215);
    log_1->poll();
    STRA_LOG(log_1, DEBUG, "%d %d %s %lf", 10, 111, "test2", 1.020215);
    log_1->poll();

    stra_logger *log_2 = new stra_logger(tscns);
    log_2->set_log_level(INFO);
    log_2->set_log_file("../test2.log");
    STRA_LOG(log_2, DEBUG, "%d %d %s %lf", 10, 111, "test", 1.020215);
    log_2->poll();
    STRA_LOG(log_2, DEBUG, "%d %d %s %.1lf", 10, 111, "test2", 1.020215);
    log_2->poll();

    FAST_LOG(DEBUG, "%d %d", 5, 5);
    fast_logger::get_logger().poll();

    auto t0 = get_current_nano_sec();
    STRA_LOG(log_1, DEBUG, "%d %d %s %lf", 10, 111, "tesdafdafdafdat", 1.020215);
    STRA_LOG(log_1, DEBUG, "%d %d %s %lf", 1100, 111, "teeeeest", 1.877);
    STRA_LOG(log_1, DEBUG, "%d %d %s %lf %s %.6lf", 1545, 111, "tesgdagfdasect", 1.11, "dadfasefdaf", 1.15555555333);
    auto t1 = get_current_nano_sec();
    std::cout << (t1 - t0) / 3.0 << std::endl;

    for (int i = 0; i < 1500; i++)
    {
        if (i == 1)
            t0 = get_current_nano_sec();
        STRA_LOG(log_1, DEBUG, "%d %d %s %lf", 10, 111, "tesdafdafdafdat", 1.020215);
        STRA_LOG(log_1, DEBUG, "%d %d %s %lf", 1100, 111, "teeeeest", 1.877);
        STRA_LOG(log_1, DEBUG, "%d %d %s %lf %s %.6lf", 1545, 111, "tesgdagfdasect", 1.11, "dadfasefdaf", 1.15555555333);
    }
    t1 = get_current_nano_sec();
    std::cout << (t1 - t0) / 4500.0 << std::endl;
    log_1->poll();
    delete log;
    delete log_1;

}