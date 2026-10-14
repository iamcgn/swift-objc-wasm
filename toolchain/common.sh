# Shared configuration for the host compiler and wasm stdlib builds.
SWIFT_SOURCE=${SWIFT_SOURCE:-$HOME/Development/swift-source}
export DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}
# The LLVM 21 sources do not compile against the macOS 27 SDK's libc++; build against
# the Command Line Tools' MacOSX26 SDK (xcrun --sdk ignores SDKROOT, so override CMake).
MACOS_SDK=${MACOS_SDK:-/Library/Developer/CommandLineTools/SDKs/MacOSX26.sdk}
export SWIFT_BUILD_MACOS_SYSROOT="$MACOS_SDK"
JOBS=${JOBS:-6}
BUILD_SUBDIR=objc-wasm
TOOLCHAIN_DESTDIR="$SWIFT_SOURCE/build/$BUILD_SUBDIR/toolchain-macosx-arm64"
COMMON_ARGS=(
  --release --assertions --build-subdir "$BUILD_SUBDIR"
  --skip-ios --skip-tvos --skip-watchos --skip-xros --skip-build-osx --skip-build-benchmarks
  --skip-test-swift --skip-test-cmark --skip-build-compiler-rt --build-swift-libexec=0 --reconfigure
  --build-swift-dynamic-stdlib=0 --build-swift-dynamic-sdk-overlay=0
  --build-swift-static-stdlib=0 --build-swift-static-sdk-overlay=0
  --llvm-targets-to-build="AArch64;X86;WebAssembly"
  # The host tools are staged into $TOOLCHAIN_DESTDIR/usr by stage-toolchain.sh (the
  # install targets would also build the macOS clang overlays).
  --install-prefix=/usr --install-destdir="$TOOLCHAIN_DESTDIR"
  --extra-cmake-options="-DLLVM_PARALLEL_LINK_JOBS=1 -DSWIFT_PARALLEL_LINK_JOBS=1 -DCMAKE_OSX_SYSROOT:PATH=$MACOS_SDK"
  --jobs "$JOBS"
)
