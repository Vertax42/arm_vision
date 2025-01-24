#ifndef VERSION_H
#define VERSION_H

#define CIRCLE_DETECTION_MAJOR 1

#define CIRCLE_DETECTION_MINOR 0

#define CIRCLE_DETECTION_PATCH 2

#define CIRCLE_DETECTION_BUILD 2

#define CIRCLE_DETECTION_VERSION_STRING "1.0.2.2"

#ifdef __cplusplus
extern "C" {
#endif
//获取主版本号
static inline int get_version_major() { return CIRCLE_DETECTION_MAJOR; }
//获取次版本号
static inline int get_version_minor() { return CIRCLE_DETECTION_MINOR; }
//获取修订号
static inline int get_version_patch() { return CIRCLE_DETECTION_PATCH; }
//获取构建号
static inline int get_version_build() { return CIRCLE_DETECTION_BUILD; }
//获取版本号
static inline const char *get_version_string() { return CIRCLE_DETECTION_VERSION_STRING; }

#ifdef __cplusplus
}
#endif
#endif // VERSION_H
