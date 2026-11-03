#include "gf.h"
const char *gf_result_nom(gf_result_t r) {
    static const char *const names[]={"OK","ARGUMENT","INDEX","CAPACITE","ABSENT","IO",
        "TIMEOUT","CRC","VERSION","VIDE","NON_CALIBRE","PLAGE","CONFLIT","OCCUPE",
        "VERIFICATION","SELECTION","NON_SUPPORTE"};
    return (unsigned)r<sizeof(names)/sizeof(names[0])?names[r]:"INCONNU";
}
uint32_t gf_crc32(const uint8_t *data,size_t n) {
    uint32_t c=UINT32_MAX;
    for(size_t i=0;i<n;i++) {
        c^=data[i];
        for(unsigned b=0;b<8;b++)c=(c>>1)^((c&1U)?UINT32_C(0xEDB88320):0);
    }
    return ~c;
}
