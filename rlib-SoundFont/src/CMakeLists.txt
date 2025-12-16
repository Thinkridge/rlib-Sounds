# CMakeList.txt : convert の CMake プロジェクト。ソースを含めて、次を定義します:
# プロジェクト専用ロジックはこちらです。
#
cmake_minimum_required (VERSION 3.8)


# デフォルトは Release ビルド
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING
	"Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
	FORCE)
endif()

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)


project ("smftowav")

# ソースをこのプロジェクトの実行可能ファイルに追加します。
add_executable (smftowav
	"./smftowav.cpp"
	"./sequencer/Smf.cpp"
	"./fm/fm2203r.cpp"
	"./fm/ymfm_adpcm.cpp"
	"./fm/ymfm_ssg.cpp"
)

# ライブラリ
TARGET_LINK_LIBRARIES(smftowav stdc++fs)
TARGET_LINK_LIBRARIES(smftowav pthread)
TARGET_LINK_LIBRARIES(smftowav rt)
TARGET_LINK_LIBRARIES(smftowav boost_program_options boost_regex boost_thread)


#project ("sfinfo")
#
## ソースをこのプロジェクトの実行可能ファイルに追加します。
#add_executable (sfinfo
#	"./sfinfo.cpp"
#)
#
# ライブラリ
#TARGET_LINK_LIBRARIES(sfinfo stdc++fs)
#TARGET_LINK_LIBRARIES(sfinfo boost_program_options boost_regex)


# TODO: テストを追加し、必要な場合は、ターゲットをインストールします。
