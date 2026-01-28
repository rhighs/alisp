#pragma once

#ifdef DEBUG
#define DBG_LOG(FMT, ...) \
    do{fprintf(stderr, "[%s:%s:%d]: " FMT "\n", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}while(0)
#else
#define DBG_LOG(FMT, ...) do{}while(0)
#endif
