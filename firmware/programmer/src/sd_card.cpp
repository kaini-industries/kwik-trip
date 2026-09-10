#ifdef ETAG_CARDPUTER_ADV
#include "sd_card.h"
#include "etag_catalog.h"
#include <SD.h>
#include <SPI.h>

namespace etag_host {
bool mountSd() {
    static SPIClass spi(HSPI);
    static bool mounted = false;
    if (mounted && SD.cardType() != CARD_NONE) return true;
    if (mounted) SD.end();
    spi.begin(etag_build::kSdClock, etag_build::kSdMiso,
              etag_build::kSdMosi, etag_build::kSdCs);
    mounted = SD.begin(etag_build::kSdCs, spi, etag_build::kSdHz);
    return mounted;
}
}
#endif
