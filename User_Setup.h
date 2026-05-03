#define USER_SETUP_INFO "ST7789 based on ST7735 wiring"

// ===================== ДРАЙВЕР =====================
#define ST7789_DRIVER

// ===================== РАЗРЕШЕНИЕ =====================
// 99% ST7789 = 240x320
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// ===================== HSPI (как у тебя было) =====================
#define USE_HSPI_PORT

#define TFT_MOSI 4
#define TFT_SCLK 5
#define TFT_CS   3
#define TFT_DC   6
#define TFT_RST  2

#define TFT_MISO -1

// ===================== ПОДСВЕТКА =====================
#define TFT_BL -1   // пока не трогаем, чтобы не было крашей

// ===================== SPI =====================
#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000

// ===================== ШРИФТЫ =====================
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT