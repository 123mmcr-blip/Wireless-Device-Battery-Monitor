# Builds WirelessBattery.exe with MinGW-w64 (MSYS2 MINGW64/UCRT64 shell, or any mingw g++ on PATH).
CXX      ?= g++
WINDRES  ?= windres
VERSION  ?= 0.0.0
CHANNEL  ?= dev
.DEFAULT_GOAL := WirelessBattery.exe
CXXFLAGS ?= -std=c++17 -Os -Wall -Wno-missing-field-initializers -ffunction-sections -fdata-sections \
            -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00
LDFLAGS  ?= -municode -mwindows -static -s -Wl,--gc-sections
LIBS     := -lhid -lsetupapi -lcfgmgr32 -lole32 -loleaut32 -lcomctl32 -lgdi32 -lruntimeobject -lws2_32 -lwinhttp -lbcrypt -lversion

SRC := $(wildcard src/*.cpp) $(wildcard src/drivers/*.cpp)
OBJ := $(patsubst src/%.cpp,build/%.o,$(SRC)) build/app_res.o

WirelessBattery.exe: $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LIBS)

build/%.o: src/%.cpp $(wildcard src/*.h src/drivers/*.h) build/version.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -municode -c $< -o $@

build/version.h: scripts/generate-version.ps1 FORCE
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/generate-version.ps1 -Version "$(VERSION)" -Channel "$(CHANNEL)"

build/app_res.o: res/app.rc res/app.manifest res/app.ico build/version.h
	@mkdir -p build
	$(WINDRES) -i $< -o $@ --include-dir res

clean:
	rm -rf build WirelessBattery.exe

test: test-update-core test-update-win
	@mkdir -p build
	$(CXX) $(CXXFLAGS) tests/astro_driver_test.cpp src/drivers/astro.cpp -o build/astro_driver_test.exe
	./build/astro_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/steelseries_mouse_test.cpp src/drivers/steelseries.cpp -o build/steelseries_mouse_test.exe
	./build/steelseries_mouse_test.exe
	$(CXX) -std=c++17 -O2 -Wall tests/rapoo_protocol_test.cpp -o build/rapoo_protocol_test.exe
	./build/rapoo_protocol_test.exe
	$(CXX) $(CXXFLAGS) tests/rapoo_driver_test.cpp src/drivers/rapoo.cpp -o build/rapoo_driver_test.exe
	./build/rapoo_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/ninjutso_driver_test.cpp src/drivers/ninjutso.cpp -o build/ninjutso_driver_test.exe
	./build/ninjutso_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/cherry_driver_test.cpp src/drivers/cherry.cpp -o build/cherry_driver_test.exe
	./build/cherry_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/ninjutso_cable_test.cpp src/drivers/ninjutso.cpp -o build/ninjutso_cable_test.exe
	./build/ninjutso_cable_test.exe
	$(CXX) $(CXXFLAGS) tests/machenike_driver_test.cpp src/drivers/machenike.cpp -o build/machenike_driver_test.exe
	./build/machenike_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/niceboy_driver_test.cpp src/drivers/niceboy.cpp -o build/niceboy_driver_test.exe
	./build/niceboy_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/alienware_driver_test.cpp src/drivers/alienware.cpp -o build/alienware_driver_test.exe
	./build/alienware_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/mchose_modern_driver_test.cpp src/drivers/mchose_modern.cpp -o build/mchose_modern_driver_test.exe
	./build/mchose_modern_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/vaxee_driver_test.cpp src/drivers/vaxee.cpp -o build/vaxee_driver_test.exe
	./build/vaxee_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/finalmouse_driver_test.cpp src/drivers/finalmouse.cpp -o build/finalmouse_driver_test.exe
	./build/finalmouse_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/keychron_driver_test.cpp src/drivers/keychron.cpp -o build/keychron_driver_test.exe
	./build/keychron_driver_test.exe
	$(CXX) $(CXXFLAGS) tests/hyperx_haste_driver_test.cpp src/drivers/hyperx.cpp -o build/hyperx_haste_driver_test.exe
	./build/hyperx_haste_driver_test.exe
	$(CXX) -std=c++17 -O2 -Wall tests/galaxy_buds_protocol_test.cpp -o build/galaxy_buds_protocol_test.exe
	./build/galaxy_buds_protocol_test.exe
	$(CXX) -std=c++17 -O2 -Wall tests/logitech_g435_0adf_test.cpp -o build/logitech_g435_0adf_test.exe
	./build/logitech_g435_0adf_test.exe
	$(CXX) -std=c++17 -O2 -Wall -Isrc tests/xenta_protocol_test.cpp src/drivers/xenta_protocol.cpp -o build/xenta_protocol_test.exe
	./build/xenta_protocol_test.exe

test-update-core:
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall tests/update_core_test.cpp src/update_core.cpp -o build/update_core_test.exe
	./build/update_core_test.exe

test-update-win: build/version.h build/app_res.o
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall tests/update_win_test.cpp src/update_core.cpp build/app_res.o \
	    -o build/update_win_test.exe -lwinhttp -lbcrypt -lcomctl32 -lversion -lole32
	./build/update_win_test.exe

FORCE:

.PHONY: clean test test-update-core test-update-win FORCE
