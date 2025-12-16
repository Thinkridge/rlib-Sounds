
cmake_minimum_required (VERSION 3.8)

# デフォルトは Release ビルド
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING
	"Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
	FORCE)
endif()


project ("rlibsoundfontdecoder")

set(CMAKE_CXX_STANDARD 17)

# スレッド不使用
add_definitions(-DDISABLE_THREADS)

# ソースをこのプロジェクトの実行可能ファイルに追加します。
add_executable ( ${CMAKE_PROJECT_NAME}
	"./main.cpp"
	"../json/Json.h"
	"../sequencer/MidiEvent.h"
	"../sequencer/Smf.cpp"
	"../sequencer/Smf.h"
	"../sequencer/SmfToWav.h"
	"../sequencer/Soundfont.h"
	"../sequencer/SoundfontInfo.h"
	"../sequencer/SoundfontMidiModule.h"
	"../sequencer/SoundfontRenderer.h"
	"../sequencer/TempoList.h"
)

set(CMAKE_EXECUTABLE_SUFFIX ".js")
#set(CMAKE_CXX_FLAGS "--bind")
#set(CMAKE_CXX_FLAGS "-std=c++17 -O0 -g3 --bind -s NO_DISABLE_EXCEPTION_CATCHING -s WASM=1 -s SINGLE_FILE=1 -s MODULARIZE=1 -s ENVIRONMENT=web")

target_compile_options( ${CMAKE_PROJECT_NAME} PRIVATE
	--bind
	-fexceptions
#	-pthread
)

target_link_options( ${CMAKE_PROJECT_NAME} PRIVATE
#	-pthread
	-lembind
	-sNO_DISABLE_EXCEPTION_CATCHING
	-sWASM=1
	-sSINGLE_FILE=1
	-sMODULARIZE=1
#	-sENVIRONMENT=web,worker
	-sENVIRONMENT='web'
	-sALLOW_MEMORY_GROWTH
	-sEXPORT_NAME=ModuleSoundfont
	#-sPTHREAD_POOL_SIZE_STRICT=4
#	-sPTHREAD_POOL_SIZE=4
	-sEXPORT_ES6=1
)
