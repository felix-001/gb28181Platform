#ifndef LOG_H
#define LOG_H

#define LOGI(fmt, args...) printf("%s:%d(%s) $ "fmt"\n", __FILENAME__, __LINE__, __FUNCTION__, ##args)
#define LOGE(fmt, args...) printf("\e[0;31m%s:%d(%s)$ "fmt"\n\e[0m", __FILENAME__, __LINE__, __FUNCTION__, ##args)

#endif

