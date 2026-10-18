#include "dashboard.h"
#include <cstdio>
#include <cstring>
#include <string>

void save(const char* name, const Dashboard& data) {
    GFXcanvas16 image(296, 128);
    drawDashboard(image, data);
    std::string path = std::string("previews/") + name + ".ppm";
    FILE* file = fopen(path.c_str(), "wb");
    fprintf(file, "P6\n296 128\n255\n");
    for (int y = 0; y < 128; ++y) for (int x = 0; x < 296; ++x) {
        const uint16_t pixel = image.getPixel(x, y);
        const unsigned char color[] = {static_cast<unsigned char>(pixel ? (pixel == 0xffff ? 255 : 220) : 0),
                                      static_cast<unsigned char>(pixel == 0xffff ? 255 : 0),
                                      static_cast<unsigned char>(pixel == 0xffff ? 255 : 0)};
        fwrite(color, 1, 3, file);
    }
    fclose(file);
}
int main() {
    Dashboard data;
    data.wifi = true; data.rssi = -60; data.devicePercent = 82;
    strcpy(data.clock, "22:31");
    data.weather.minimum=7; data.weather.maximum=16; data.weather.precipitation=10; data.weather.code=0;
    data.solar.pv=1; data.solar.grid=2; data.solar.load=-276; data.solar.battery=311; data.solar.soc=71;
    data.solar.backupKnown=true; data.solar.backup=false;
    save("normal", data);
    data.solar.backup=true;
    save("backup", data);
    data.solar.backup=false; data.solar.pv=5420; data.solar.grid=-1230; data.solar.load=-1560;
    data.solar.battery=-2630; data.solar.soc=33; data.devicePercent=9; data.weather.code=61;
    save("charging", data);
    data.solar.pv=2400; data.solar.grid=1230; data.solar.load=-3030;
    data.solar.battery=-600; data.solar.soc=50;
    save("import_charging", data);
    data.solar.pv=1200; data.solar.grid=-500; data.solar.load=-1500;
    data.solar.battery=800; data.solar.soc=25;
    save("export_discharging", data);
    data.solar.pv=0; data.solar.grid=0; data.solar.load=0;
    data.solar.battery=0; data.solar.soc=0;
    save("idle", data);
    data.solar.pv=12500; data.solar.grid=10120; data.solar.load=-22500; data.solar.battery=999.9;
    data.solar.soc=100; data.weather.minimum=-18; data.weather.maximum=-8;
    data.weather.precipitation=100; data.weather.code=95; data.devicePercent=100;
    save("large_values", data);
    save("unavailable", Dashboard{});
    for (int code : {0,1,2,3,45,48,51,53,55,56,57,61,63,65,66,67,71,73,75,77,80,81,82,85,86,95,96,99}) {
        data.weather.code=code;
        save((std::string("weather_")+std::to_string(code)).c_str(), data);
    }
}
