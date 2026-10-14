#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0
#
# Builds the third-party libraries the core needs for Android (arm64-v8a), with the same
# versions and checksums as build-dependencies-linux.sh, less lunasvg (its fork is gone, and
# imgui only needs it for SVG fonts). The libraries are static, except
# shaderc and SPIRV-Cross, which the core opens at runtime and which go in the APK.
#
# Syntax: build-dependencies-android.sh <android ndk> <output directory>

set -e

if [ "$#" -ne 2 ]; then
	echo "Syntax: $0 <android ndk> <output directory>"
	exit 1
fi

NDK="$1"
INSTALLDIR="$2"
if [ "${INSTALLDIR:0:1}" != "/" ]; then
	INSTALLDIR="$PWD/$INSTALLDIR"
fi
if [ ! -f "$NDK/build/cmake/android.toolchain.cmake" ]; then
	echo "Not an Android NDK: $NDK"
	exit 1
fi

ABI=arm64-v8a
API=29
NPROCS="$(getconf _NPROCESSORS_ONLN)"

FREETYPE=2.13.3
LIBJPEGTURBO=3.0.3
LIBPNG=1.6.43
LIBWEBP=1.4.0
ZSTD=1.5.6

CPUINFO=7524ad504fdcfcf75a18a133da6abd75c5d48053
SHADERC=f60bb80e255144e71776e2ad570d89b78ea2ab4f
SOUNDTOUCH=463ade388f3a51da078dc9ed062bf28e4ba29da7
SPIRV_CROSS=vulkan-sdk-1.3.290.0

mkdir -p "$INSTALLDIR/deps-build"
cd "$INSTALLDIR/deps-build"

curl -C - -L \
	-o "cpuinfo-$CPUINFO.tar.gz" "https://github.com/stenzek/cpuinfo/archive/$CPUINFO.tar.gz" \
	-o "shaderc-$SHADERC.tar.gz" "https://github.com/stenzek/shaderc/archive/$SHADERC.tar.gz" \
	-o "soundtouch-$SOUNDTOUCH.tar.gz" "https://github.com/stenzek/soundtouch/archive/$SOUNDTOUCH.tar.gz" \
	-o "SPIRV-Cross-$SPIRV_CROSS.tar.gz" "https://github.com/KhronosGroup/SPIRV-Cross/archive/refs/tags/$SPIRV_CROSS.tar.gz" \
	-o "freetype-$FREETYPE.tar.xz" "https://sourceforge.net/projects/freetype/files/freetype2/$FREETYPE/freetype-$FREETYPE.tar.xz/download" \
	-O "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$LIBJPEGTURBO/libjpeg-turbo-$LIBJPEGTURBO.tar.gz" \
	-O "https://downloads.sourceforge.net/project/libpng/libpng16/$LIBPNG/libpng-$LIBPNG.tar.xz" \
	-O "https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-$LIBWEBP.tar.gz" \
	-O "https://github.com/facebook/zstd/releases/download/v$ZSTD/zstd-$ZSTD.tar.gz"

cat > SHASUMS <<EOF
e1351218d270db49c3dddcba04fb2153b09731ea3fa6830e423f5952f44585be  cpuinfo-$CPUINFO.tar.gz
4c1780b6c65c27c4dcb109f08ab632241c98b77fe2e22be726c151ff514482bf  shaderc-$SHADERC.tar.gz
fe45c2af99f6102d2704277d392c1c83b55180a70bfd17fb888cc84a54b70573  soundtouch-$SOUNDTOUCH.tar.gz
1333fd2a05ab8a0572106e3b7fb8161ea0b67ab7d0a1b8bdd14b47f89ac8a611  SPIRV-Cross-$SPIRV_CROSS.tar.gz
0550350666d427c74daeb85d5ac7bb353acba5f76956395995311a9c6f063289  freetype-$FREETYPE.tar.xz
343e789069fc7afbcdfe44dbba7dbbf45afa98a15150e079a38e60e44578865d  libjpeg-turbo-$LIBJPEGTURBO.tar.gz
6a5ca0652392a2d7c9db2ae5b40210843c0bbc081cbd410825ab00cc59f14a6c  libpng-$LIBPNG.tar.xz
61f873ec69e3be1b99535634340d5bde750b2e4447caa1db9f61be3fd49ab1e5  libwebp-$LIBWEBP.tar.gz
8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1  zstd-$ZSTD.tar.gz
EOF
shasum -a 256 --check SHASUMS

# The NDK toolchain only searches inside CMAKE_FIND_ROOT_PATH, so the install directory goes
# there as well as in CMAKE_PREFIX_PATH.
CMAKE_COMMON=(
	-G Ninja
	-DCMAKE_BUILD_TYPE=Release
	-DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake"
	-DANDROID_ABI=$ABI
	-DANDROID_PLATFORM=android-$API
	-DANDROID_STL=c++_static
	-DCMAKE_PREFIX_PATH="$INSTALLDIR"
	-DCMAKE_FIND_ROOT_PATH="$INSTALLDIR"
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR"
	-DCMAKE_POSITION_INDEPENDENT_CODE=ON
	-DBUILD_SHARED_LIBS=OFF
)

build() {
	local dir="$1"
	shift
	cmake "${CMAKE_COMMON[@]}" "$@" -B "$dir/build-android" -S "$dir"
	cmake --build "$dir/build-android" --parallel "$NPROCS"
	cmake --install "$dir/build-android"
}

unpack() {
	rm -fr "$2"
	tar xf "$1"
}

echo "Building Zstandard..."
unpack "zstd-$ZSTD.tar.gz" "zstd-$ZSTD"
build "zstd-$ZSTD/build/cmake" -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_STATIC=ON -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF

echo "Building libpng..."
unpack "libpng-$LIBPNG.tar.xz" "libpng-$LIBPNG"
build "libpng-$LIBPNG" -DPNG_TESTS=OFF -DPNG_STATIC=ON -DPNG_SHARED=OFF -DPNG_TOOLS=OFF -DPNG_ARM_NEON=on

echo "Building libjpeg..."
unpack "libjpeg-turbo-$LIBJPEGTURBO.tar.gz" "libjpeg-turbo-$LIBJPEGTURBO"
build "libjpeg-turbo-$LIBJPEGTURBO" -DENABLE_STATIC=ON -DENABLE_SHARED=OFF -DWITH_TURBOJPEG=OFF

echo "Building WebP..."
unpack "libwebp-$LIBWEBP.tar.gz" "libwebp-$LIBWEBP"
build "libwebp-$LIBWEBP" \
	-DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF \
	-DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_EXTRAS=OFF

echo "Building FreeType..."
unpack "freetype-$FREETYPE.tar.xz" "freetype-$FREETYPE"
build "freetype-$FREETYPE" -DFT_REQUIRE_ZLIB=ON -DFT_REQUIRE_PNG=ON -DFT_DISABLE_BZIP2=TRUE -DFT_DISABLE_BROTLI=TRUE -DFT_DISABLE_HARFBUZZ=TRUE

echo "Building cpuinfo..."
unpack "cpuinfo-$CPUINFO.tar.gz" "cpuinfo-$CPUINFO"
build "cpuinfo-$CPUINFO" -DCPUINFO_LIBRARY_TYPE=static -DCPUINFO_RUNTIME_TYPE=static -DCPUINFO_LOG_LEVEL=error \
	-DCPUINFO_BUILD_TOOLS=OFF -DCPUINFO_BUILD_UNIT_TESTS=OFF -DCPUINFO_BUILD_MOCK_TESTS=OFF -DCPUINFO_BUILD_BENCHMARKS=OFF \
	-DUSE_SYSTEM_LIBS=ON

echo "Building soundtouch..."
unpack "soundtouch-$SOUNDTOUCH.tar.gz" "soundtouch-$SOUNDTOUCH"
build "soundtouch-$SOUNDTOUCH"

echo "Building shaderc..."
unpack "shaderc-$SHADERC.tar.gz" "shaderc-$SHADERC"
build "shaderc-$SHADERC" -DSHADERC_SKIP_TESTS=ON -DSHADERC_SKIP_EXAMPLES=ON -DSHADERC_SKIP_COPYRIGHT_CHECK=ON

echo "Building SPIRV-Cross..."
unpack "SPIRV-Cross-$SPIRV_CROSS.tar.gz" "SPIRV-Cross-$SPIRV_CROSS"
build "SPIRV-Cross-$SPIRV_CROSS" -DSPIRV_CROSS_SHARED=ON -DSPIRV_CROSS_STATIC=OFF -DSPIRV_CROSS_CLI=OFF \
	-DSPIRV_CROSS_ENABLE_TESTS=OFF -DSPIRV_CROSS_ENABLE_GLSL=ON -DSPIRV_CROSS_ENABLE_HLSL=OFF -DSPIRV_CROSS_ENABLE_MSL=OFF \
	-DSPIRV_CROSS_ENABLE_CPP=OFF -DSPIRV_CROSS_ENABLE_REFLECT=OFF -DSPIRV_CROSS_ENABLE_C_API=ON -DSPIRV_CROSS_ENABLE_UTIL=ON

echo "Cleaning up..."
cd ..
rm -fr deps-build

echo "Done."
