#ifndef GF_H
#define GF_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Erreurs transportables sur PC et cible ; jamais une mesure physique. */
typedef enum {
    GF_OK=0, GF_ARGUMENT, GF_INDEX, GF_CAPACITE, GF_ABSENT, GF_IO,
    GF_TIMEOUT, GF_CRC, GF_VERSION, GF_VIDE, GF_NON_CALIBRE, GF_PLAGE,
    GF_CONFLIT, GF_OCCUPE, GF_VERIFICATION, GF_SELECTION, GF_NON_SUPPORTE
} gf_result_t;
const char *gf_result_nom(gf_result_t r);
uint32_t gf_crc32(const uint8_t *data, size_t n);
static inline uint16_t gf_u16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1]<<8));
}
static inline uint32_t gf_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline int32_t gf_i32(const uint8_t *p) {
    uint32_t u=gf_u32(p);
    return u<=INT32_MAX ? (int32_t)u : (int32_t)(-1-(int64_t)(UINT32_MAX-u));
}
static inline void gf_p16(uint8_t *p,uint16_t u) {p[0]=(uint8_t)u;p[1]=(uint8_t)(u>>8);}
static inline void gf_p32(uint8_t *p,uint32_t u) {
    for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(u>>(8*i));
}
#endif
