#include "fast_logger.h"

thread_local staging_buffer                       *fast_logger::staging_buffer_ = nullptr;
thread_local fast_logger::staging_buffer_destroyer fast_logger::sbc_;

void fast_logger::ensure_staging_buffer_allocated()
{
    if (staging_buffer_ == nullptr) {
        std::unique_lock<std::mutex> guard(buffer_mutex_);
        guard.unlock();
        staging_buffer_ = new staging_buffer();
        sbc_.staging_buffer_created();
        guard.lock();
        thread_buffers_.push_back(staging_buffer_);
    }
}

staging_buffer::queue_header *fast_logger::alloc(size_t size)
{
    if (staging_buffer_ == nullptr) {
        ensure_staging_buffer_allocated();
    }
    return staging_buffer_->alloc(size);
}

void fast_logger::finish()
{
    staging_buffer_->finish();
}

void fast_logger::register_log(static_log_info &info, int &log_id)
{
    std::lock_guard<std::mutex> lock(register_mutex_);
    if (log_id != UNASSIGNED_LOGID)
        return;
    char *p = static_cast<char *>(malloc(1024));
    info.create_log_fragments(&p);
    log_id = static_cast<int32_t>(log_infos_.size()) + static_cast<int32_t>(log_handler_.get_log_infos_cnt());
    log_infos_.push_back(info);
}

void fast_logger::adjust_heap(size_t i)
{
    while (true) {
        size_t min_i = i;
        size_t ch    = std::min(i * 2 + 1, bg_thread_buffers_.size());
        size_t end   = std::min(ch + 2, bg_thread_buffers_.size());
        for (; ch < end; ch++) {
            auto h_ch  = bg_thread_buffers_[ch].header;
            auto h_min = bg_thread_buffers_[min_i].header;
            if (h_ch && (!h_min || *(int64_t *)(h_ch + 1) < *(int64_t *)(h_min + 1)))
                min_i = ch;
        }
        if (min_i == i)
            break;
        std::swap(bg_thread_buffers_[i], bg_thread_buffers_[min_i]);
        i = min_i;
    }
}

void fast_logger::poll_inner()
{
    int64_t tsc = tscns_.rdtsc();
    if (log_infos_.size()) {
        std::unique_lock<std::mutex> lock(register_mutex_);
        for (auto info : log_infos_) {
            log_handler_.add_log_info(info);
        }
        log_infos_.clear();
    }
    if (thread_buffers_.size()) {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        for (auto tb : thread_buffers_) {
            bg_thread_buffers_.emplace_back(tb);
        }
        thread_buffers_.clear();
    }
    for (size_t i = 0; i < bg_thread_buffers_.size(); i++) {
        auto &node = bg_thread_buffers_[i];
        if (node.header)
            continue;
        node.header = node.tb->front();
        if (!node.header && node.tb->check_can_delete()) {
            delete node.tb;
            node = bg_thread_buffers_.back();
            bg_thread_buffers_.pop_back();
            i--;
        }
    }
    if (bg_thread_buffers_.empty())
        return;

    // build heap
    for (int i = bg_thread_buffers_.size() / 2; i >= 0; i--) {
        adjust_heap(i);
    }
    while (true) {
        auto h = bg_thread_buffers_[0].header;
        if (!h || h->userdata >= log_handler_.get_log_infos_cnt() || *(int64_t *)(h + 1) >= tsc)
            break;
        auto tb = bg_thread_buffers_[0].tb;
        log_handler_.handle_log(tb->get_name(), h);
        tb->pop();
        bg_thread_buffers_[0].header = tb->front();
        adjust_heap(0);
    }
}
