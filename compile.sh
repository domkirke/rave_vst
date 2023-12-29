CONFIG=Release
clean=0
build=1

while [ $# -gt 0 ]; do
  case "$1" in
    --config=*)
      CONFIG="${1#*=}"
      ;;
    --clean=*)
      clean="${1#*=}"
      ;;
    --build=*)
      build="${1#*=}"
      ;;
    *)
      printf "***************************\n"
      printf "* Error: Invalid argument.*\n"
      printf "***************************\n"
      exit 1
  esac
  shift
done


if [ $clean -eq 1 ]; then
    # Clean #
    rm -rf build
    rm -rf Libs/JUCE-*
fi

if [ $build -eq 1 ]; then
    # Make build dir and copy libraries #
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=${CONFIG} -DCMAKE_OSX_ARCHITECTURES=x86_64\;arm64
    cmake --build . --config ${CONFIG} -j 4

  if [[ "$OSTYPE" =~ ^darwin* ]]; then
    # codesign locally #
    codesign -f --timestamp -s - rave-vst_artefacts/${CONFIG}/AU/RAVE.component/Contents/MacOS/RAVE
    codesign -f --timestamp -s - rave-vst_artefacts/${CONFIG}/VST3/RAVE.vst3/Contents/MacOS/RAVE
    codesign -f --timestamp -s - rave-vst_artefacts/${CONFIG}/Standalone/RAVE.app/Contents/MacOS/RAVE
  fi

fi