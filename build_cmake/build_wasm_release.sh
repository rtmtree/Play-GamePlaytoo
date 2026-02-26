#!/bin/sh
script_dir=$(dirname -- "$0")
pushd ..
cmake --build --preset wasm-ninja-release
popd
cp $script_dir/build/wasm-ninja/Source/ui_js/Play900.js $script_dir/../js/play_browser/src/Play900.js
cp $script_dir/build/wasm-ninja/Source/ui_js/Play900.js $script_dir/../js/play_browser/public/Play900.js
cp $script_dir/build/wasm-ninja/Source/ui_js/Play900.wasm $script_dir/../js/play_browser/public/Play900.wasm
cp $script_dir/build/wasm-ninja/Source/ui_js/Play900.wasm $script_dir/../js/play_browser/src/Play900.wasm
cp $script_dir/build/wasm-ninja/tools/PsfPlayer/Source/ui_js/PsfPlayer.js $script_dir/../js/psfplayer_browser/src/PsfPlayer.js
cp $script_dir/build/wasm-ninja/tools/PsfPlayer/Source/ui_js/PsfPlayer.js $script_dir/../js/psfplayer_browser/public/PsfPlayer.js
cp $script_dir/build/wasm-ninja/tools/PsfPlayer/Source/ui_js/PsfPlayer.wasm $script_dir/../js/psfplayer_browser/public/PsfPlayer.wasm
