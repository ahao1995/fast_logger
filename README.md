# fast_logger高性能异步日志库

基于c++11高性能异步日志库，提供两种场景api

场景1：单进程记录一个文件，即logger为singleton，logger为线程安全的，需要一个后台线程进行polling进行日志写入
```
FAST_LOG(DEBUG, "%d %d", 5, 10);
fast_logger::get_logger().poll();
```
场景2：单进程中需要多个logger，每个logger是线程安全的，需要一个后台线程对所有的logger进行polling日志写入
```
TSCNS tscns;
stra_logger *log = new stra_logger(tscns);
log->set_log_file("../test.log");
STRA_LOG(log, DEBUG, "%d %d %s %lf", 10, 111, "test", 1.11);
log->poll();
```
代码参考：

1. https://github.com/MengRao/NanoLogLite

2. https://github.com/MengRao/tscns

3. https://github.com/MengRao/fmtlog

4. https://github.com/MengRao/SPSC_Queue


