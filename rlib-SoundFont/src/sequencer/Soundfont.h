#pragma once

#include <set>

#include "../base/Riff.h"

// #define SOUNDFONT_COPYABLE	コピーを可にする(意図しないコピーが走らないよう注意すべし)

namespace rlib::soundfont {

	namespace math {

		// std::pow(10, n) の高速版(要確認)
		// https://openaudio.blogspot.com/2017/02/faster-log10-and-pow.html
		template <class T> T pow10(T f) {
			return std::exp(static_cast<T>(2.302585092994046) * f);
		}

#if 0	// 未使用
		template <class T> T log2(T x) {
			int exp;
			const auto frac = std::frexp(std::fabs(x), &exp);
			T f = 1.23149591368684;
			f *= frac;
			f += -4.11852516267426;
			f *= frac;
			f += 6.02197014179219;
			f *= frac;
			f += -3.13396450166353;
			f += exp;
			return f;
		}
		// std::log10 の 高速版
		template <class T> T log10(T x) {
			return log2(x) * 0.3010299956639812;
		}
#endif

		// dB値から振幅値(0～1.0)を得る計算式
		template <class T> T decibelsToAmplitude(T db) {
			constexpr T recip20 = static_cast<T>(1.0) / 20;
			return pow10(db * recip20);		// std::pow(10, db / 20.0);
		}

	}

	enum class GenOperator : uint16_t {
		startAddrsOffset = 0,		// サンプル		サンプルヘッダの音声波形データ開始位置に加算されるオフセット(下位16bit）
		endAddrsOffset,				// サンプル		サンプルヘッダの音声波形データ終了位置に加算されるオフセット(下位16bit）
		startloopAddrsOffset,		// サンプル		サンプルヘッダの音声波形データループ開始位置に加算されるオフセット(下位16bit）
		endloopAddrsOffset,			// サンプル		サンプルヘッダの音声波形データループ終了位置に加算されるオフセット(下位16bit）
		startAddrsCoarseOffset,		// サンプル		サンプルヘッダの音声波形データ開始位置に加算されるオフセット(上位16bit）
		modLfoToPitch,				// LFO			LFOによるピッチの揺れ幅
		vibLfoToPitch,				// ホイール		モジュレーションホイール用LFOからピッチに対しての影響量
		modEnvToPitch,				// フィルタ		フィルタ・ピッチ用エンベロープからピッチに対しての影響量
		initialFilterFc,			// フィルタ		フィルタのカットオフ周波数
		initialFilterQ,				// フィルタ		フィルターのQ値(レゾナンス)
		modLfoToFilterFc,			// LFO			LFOによるフィルターカットオフ周波数の揺れ幅
		modEnvToFilterFc,			// フィルタ		フィルタ・ピッチ用エンベロープからフィルターカットオフに対しての影響量
		endAddrsCoarseOffset,		// サンプル		サンプルヘッダの音声波形データ終了位置に加算されるオフセット(上位16bit）
		modLfoToVolume,				// LFO			LFOによるボリュームの揺れ幅
		unused1,					// 未使用		未使用
		chorusEffectsSend,			// エフェクト	コーラスエフェクトのセンドレベル
		reverbEffectsSend,			// エフェクト	リバーブエフェクトのセンドレベル
		pan,						// アンプ		パンの位置
		unused2,					// 未使用		未使用
		unused3,					// 未使用		未使用
		unused4,					// 未使用		未使用
		delayModLFO,				// LFO			LFOの揺れが始まるまでの時間
		freqModLFO,					// LFO			LFOの揺れの周期
		delayVibLFO,				// ホイール		ホイールの揺れが始まるまでの時間
		freqVibLFO,					// ホイール		ホイールの揺れの周期
		delayModEnv,				// フィルタ		フィルタ・ピッチ用エンベロープのディレイ(アタックが始まるまでの時間)
		attackModEnv,				// フィルタ		フィルタ・ピッチ用エンベロープのアタック時間
		holdModEnv,					// フィルタ		フィルタ・ピッチ用エンベロープのホールド時間(アタックが終わってからディケイが始まるまでの時間）
		decayModEnv,				// フィルタ		フィルタ・ピッチ用エンベロープのディケイ時間
		sustainModEnv,				// フィルタ		フィルタ・ピッチ用エンベロープのサステイン量
		releaseModEnv,				// フィルタ		フィルタ・ピッチ用エンベロープのリリース時間
		keynumToModEnvHold,			// フィルタ		キー(ノートNo)によるフィルタ・ピッチ用エンベロープのホールド時間への影響
		keynumToModEnvDecay,		// フィルタ		キー(ノートNo)によるフィルタ・ピッチ用エンベロープのディケイ時間への影響
		delayVolEnv,				// アンプ		アンプ用エンベロープのディレイ(アタックが始まるまでの時間)
		attackVolEnv,				// アンプ		アンプ用エンベロープのアタック時間
		holdVolEnv,					// アンプ		アンプ用エンベロープのホールド時間 (アタックが終わってからディケイが始まるまでの時間）
		decayVolEnv,				// アンプ		アンプ用エンベロープのディケイ時間
		sustainVolEnv,				// アンプ		アンプ用エンベロープのサステイン量 0～144.0db
		releaseVolEnv,				// アンプ		アンプ用エンベロープのリリース時間
		keynumToVolEnvHold,			// アンプ		キー(ノートNo)によるアンプ用エンベロープのホールド時間への影響
		keynumToVolEnvDecay,		// アンプ		キー(ノートNo)によるアンプ用エンベロープのディケイ時間への影響
		instrument,					// その他		割り当てるインストルメント(楽器)
		reserved1,					// 未使用		未使用
		keyRange,					// その他		マッピングするキー(ノートNo)の範囲
		velRange,					// その他		マッピングするベロシティの範囲
		startloopAddrsCoarseOffset,	// サンプル		サンプルヘッダの音声波形データループ開始位置に加算されるオフセット(上位16bit）
		keynum,						// その他		どのキー(ノートNo)でも強制的に指定したキー(ノートNo)に変更する
		velocity,					// その他		どのベロシティでも強制的に指定したベロシティに変更する
		initialAttenuation,			// アンプ		調整する音量	音量を下げる 0～144.0db
		reserved2,					// 未使用		未使用
		endloopAddrsCoarseOffset,	// サンプル		サンプルヘッダの音声波形データループ終了位置に加算されるオフセット(上位16bit）
		coarseTune,					// サンプル		半音単位での音程の調整		-120(-10oct)～120(+10oct)
		fineTune,					// サンプル		cent単位での音程の調整		-99～99
		sampleID,					// その他		割り当てるサンプル(音声波形)
		sampleModes,				// その他		サンプル(音声波形)をループさせるか等のフラグ
		reserved3,					// 未使用		未使用
		scaleTuning,				// その他		キー(ノートNo)が + 1されるごとに音程を何centあげるかの音階情報		0～1200
		exclusiveClass,				// その他		同時に音を鳴らさないようにするための排他ID(ハイハットのOpen、Close等に使用)
		overridingRootKey,			// サンプル		サンプル(音声波形)の音程の上書き情報
		unused5,					// 未使用		未使用
		endOper,					// その他		最後を示すオペレータ
	};

	enum class enumSampleMode : uint16_t {		// sampleModes
		notloop = 0,					// ループなし indicates a sound reproduced with no loop,
		loop = 1,						// ループあり indicates a sound which loops continuously,
		notloopUnused = 2,				// 2 is unused but should be interpreted as indicating no loop, and
		keyloop = 3,					// indicates a sound which loops for the duration of key depression then proceeds to play the remainder of the sample.
	};

	enum class SFSampleLink : uint16_t {
		monoSample = 1,				// モノラル
		rightSample = 2,			// 右
		leftSample = 4,				// 左
		linkedSample = 8,			// リンクサンプル(未定義)
		RomMonoSample = 0x8001,
		RomRightSample = 0x8002,
		RomLeftSample = 0x8004,
		RomLinkedSample = 0x8008,
		stereo = rightSample | leftSample,
	};

	using GeneratorMap = std::map<GenOperator, int16_t>;

#pragma pack( push )
#pragma pack( 1 )
	struct FileInfo{
		struct {			// SoundFontの各バージョンデータ用構造体
			uint16_t	major = 0;		// メジャーバージョン
			uint16_t	minor = 0;		// マイナーバージョン
		}ifil;
		std::string INAM;			// SoundFontファイルの名前
		std::string isng;			// 対象ウェーブテーブル音源チップ名
		std::string irom;			// 依存しているROM(ハードウェア)
		std::optional<decltype(ifil)> iver;	// 依存しているROM(ハードウェア)バージョン
		std::string ICRD;			// 作成日時情報
		std::string IENG;			// 製作者名
		std::string IPRD;			// 対象ウェーブテーブル音源チップを搭載した製品名
		std::string ICOP;			// 著作権者情報
		std::string ICMT;			// コメント
		std::string ISFT;			// 作成時のツール名
	};
#pragma pack( pop )

	// SoundFont パーサー (素の情報を取り扱う)
	class Parse {
	public:
#pragma pack( push )
#pragma pack( 1 )
		struct SFMod {						// pmod,imodチャンク用の構造体
			uint16_t	srcOper = 0;								// モジュレーション元(MIDI CCやピッチベンドなど)
			GenOperator	destOper = GenOperator::startAddrsOffset;	// 操作するジェネレータID
			int16_t		modAmount = 0;								// 操作するジェネレータ量
			uint16_t	amtSrcOper = 0;								// モジュレーション元その2(ピッチベンドレンジなど)
			uint16_t	modTransOper = 0;							// 変化量は線形か？曲線か？
		};

		struct SFPresetHeader {				// phdrチャンク用の構造体
			int8_t		name[20];
			uint16_t	presetno = 0;
			uint16_t	bank = 0;
			uint16_t	bagIndex = 0;
			uint32_t	library = 0;	//予約
			uint32_t	genre = 0;		//予約
			uint32_t	morph = 0;		//予約

			std::string getName()const {
				std::vector<int8_t> v(std::begin(name), std::begin(name) + std::size(name));
				v.emplace_back();		// EOS
				return std::string(reinterpret_cast<const char*>(v.data()));
			}
		};

		struct SFSampleHeader {				// shdrチャンク用の構造体
			int8_t			achSampleName[20];
			uint32_t		dwStart;			// 音声波形データの開始位置(単位：サンプル)
			uint32_t		dwEnd;				// 音声波形データの終了位置(単位：サンプル)
			uint32_t		dwStartloop;		// 音声波形データのループ開始位置(単位：サンプル)
			uint32_t		dwEndloop;			// 音声波形データのループ終了位置(単位：サンプル)
			uint32_t		dwSampleRate;		// 音声波形データのサンプリングレート
			uint8_t			byOriginalPitch;	// オリジナルの音程
			int8_t			chPitchCorrection;	// オリジナルの音程に対してのcent単位のチューニング
			uint16_t		wSampleLink;		// typeが1の場合…0、2(4)の場合…左(右)のSFSampleHeaderへのインデックス TODO:よくわからんので未対応
			SFSampleLink	sfSampleType;		// 音声波形データのタイプ TODO:よくわからんので未対応

			std::string getName()const {
				std::vector<int8_t> v(std::begin(achSampleName), std::begin(achSampleName) + std::size(achSampleName));
				v.emplace_back();		// EOS
				return std::string(reinterpret_cast<const char*>(v.data()));
			}
		};
#pragma pack( pop )

		struct Gen {			// プリセットバッグ
			std::shared_ptr<const GeneratorMap>	generator = std::make_shared<GeneratorMap>();
			std::vector<SFMod>					mods;
		};

		struct Doc {
			FileInfo info;
			struct PresetHeader : SFPresetHeader {	// プリセットヘッダ
				std::vector<Gen>	bags;			// プリセットバック
			};
			std::vector<PresetHeader>	presetHeaders;
			struct InstHeader {			// インストルメントヘッダ
				std::string			name;
				std::vector<Gen>	bags;	// インストルメントバック
			};
			std::vector<InstHeader>	instHeaders;
			std::vector<SFSampleHeader>	shdr;
			std::vector<int16_t>	smpl;
			std::vector<int8_t>		sm24;

			Doc() = default;
			Doc(Doc&&) = default;
			Doc& operator=(Doc&& s) = default;
			Doc(const Doc&) = delete;
			Doc& operator=(const Doc&) = delete;
		};

		Parse(Parse&& s) = default;
		Parse& operator=(Parse&& s) = default;
		Parse(const Parse&) = delete;
		Parse& operator=(const Parse&) = delete;

		static Parse fromStream(std::istream& is) {
			using namespace riff;
			const auto id = inner::toArray<4>;

			Doc doc;
#pragma pack( push )
#pragma pack( 1 )
			struct SFBag {	// pbag,ibagチャンク用の構造体
				uint16_t	genIndex = 0;
				uint16_t	modIndex = 0;
			};
			struct SFGen {	// pgen,igenチャンク用の構造体
				uint16_t	genOper;
				int16_t		genAmount;
			};
			struct SFInst {	// instチャンク用の構造体(アライメントに注意)
				int8_t		name[20];
				uint16_t	bagIndex;
			};
#pragma pack( pop )
			std::vector<SFPresetHeader> phdrs;
			std::vector<SFBag> pbags, ibags;
			std::vector<SFGen> pgens, igens;
			std::vector<SFInst> insts;
			std::vector<SFMod> pmods, imods;

			const auto read = [](std::istream& is, void* dst, std::streamsize size, const std::string& errMessage) {
				if (is.read(reinterpret_cast<char*>(dst), size).gcount() < size) {
					throw std::runtime_error(errMessage);
				}
			};
			const auto readString = [&read](std::istream& is, std::string& s, std::size_t size, const std::string& errMessage) {
				s.resize(size);
				read(is, s.data(), s.size(), errMessage);	// 末尾の \0 も含めてコピー( \0 がないケースを考慮)
				s.resize(std::strlen(s.c_str()));			// \0 以降を削除
			};
			const std::map<std::vector<std::array<int8_t, 4>>, std::function<void(const ChunkHead&, std::istream&)>> map = {
				{{id("sfbk"),id("INFO"),id("ifil")},[&](const ChunkHead& h,auto& is) {		// ファイルが基づくSoundFont規格バージョン
					if (h.size != sizeof(doc.info.ifil)) throw std::runtime_error("ifil chunk error");
					read(is,&doc.info.ifil,sizeof(doc.info.ifil),"ifil chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("isng")},[&](const ChunkHead& h,auto& is) {		// 対象ウェーブテーブル音源チップ名
					readString(is,doc.info.isng,h.size,"isng chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("INAM")},[&](const ChunkHead& h,auto& is) {		// SoundFontファイルの名前
					readString(is,doc.info.INAM,h.size,"INAM chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("irom")},[&](const ChunkHead& h,auto& is) {		// 依存しているROM(ハードウェア)
					readString(is,doc.info.irom,h.size,"irom chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("iver")},[&](const ChunkHead& h,auto& is) {		// 依存しているROM(ハードウェア)バージョン
					if (h.size != sizeof(*doc.info.iver)) throw std::runtime_error("iver chunk error");
					doc.info.iver.emplace();
					read(is,&*doc.info.iver,sizeof(*doc.info.iver),"iver chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("ICRD")},[&](const ChunkHead& h,auto& is) {		// 作成日時情報
					readString(is,doc.info.ICRD,h.size,"ICRD chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("IENG")},[&](const ChunkHead& h,auto& is) {		// 製作者名
					readString(is,doc.info.IENG,h.size,"IENG chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("IPRD")},[&](const ChunkHead& h,auto& is) {		// 対象ウェーブテーブル音源チップを搭載した製品名
					readString(is,doc.info.IPRD,h.size,"IPRD chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("ICOP")},[&](const ChunkHead& h,auto& is) {		// 著作権者情報
					readString(is,doc.info.ICOP,h.size,"ICOP chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("ICMT")},[&](const ChunkHead& h,auto& is) {		// コメント
					readString(is,doc.info.ICMT,h.size,"ICMT chunk error");
				}},
				{{id("sfbk"),id("INFO"),id("ISFT")},[&](const ChunkHead& h,auto& is) {		// 作成時のツール名
					readString(is,doc.info.ISFT,h.size,"ISFT chunk error");
				}},
				{{id("sfbk"),id("sdta"),id("smpl")},[&](const ChunkHead& h,auto& is) {		// 16bit モノラルで録音された波形データ
					doc.smpl.resize((h.size + 1) / 2);
					read(is, doc.smpl.data(), h.size, "smpl chunk error");
				}},
				{{id("sfbk"),id("sdta"),id("sm24")},[&](const ChunkHead& h,auto& is) {		// 波形データを24bitに拡張するための下位8bitデータ
					doc.sm24.resize(h.size);
					read(is, doc.sm24.data(), doc.sm24.size(), "sm24 chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("phdr")},[&](const ChunkHead& h,auto& is) {		// プリセット(音色)のヘッダ情報
					phdrs.resize(h.size / sizeof(SFPresetHeader));
					read(is, phdrs.data(), phdrs.size() * sizeof(phdrs[0]), "phdr chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("pbag")},[&](const ChunkHead& h,auto& is) {		// プリセット(音色)のバッグ情報
					pbags.resize(h.size / sizeof(SFBag));
					read(is, pbags.data(), pbags.size() * sizeof(pbags[0]), "pbag chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("pgen")},[&](const ChunkHead& h,auto& is) {		// プリセット(音色)のジェネレータ情報
					pgens.resize(h.size / sizeof(SFGen));
					read(is, pgens.data(), pgens.size() * sizeof(pgens[0]), "pgen chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("pmod")},[&](const ChunkHead& h,auto& is) {		// プリセット(音色)のモジュレーション情報
					pmods.resize(h.size / sizeof(SFMod));
					read(is, pmods.data(), pmods.size() * sizeof(pmods[0]), "pmod chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("imod")},[&](const ChunkHead& h,auto& is) {		// インストルメント(楽器)のモジュレーション情報
					imods.resize(h.size / sizeof(SFMod));
					read(is, imods.data(), imods.size() * sizeof(imods[0]), "imod chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("inst")},[&](const ChunkHead& h,auto& is) {		// インストルメント(楽器)のヘッダ情報
					insts.resize(h.size / sizeof(SFInst));
					read(is, insts.data(), insts.size() * sizeof(insts[0]), "inst chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("ibag")},[&](const ChunkHead& h,auto& is) {		// インストルメント(楽器)のバッグ情報
					ibags.resize(h.size / sizeof(SFBag));
					read(is, ibags.data(), ibags.size() * sizeof(ibags[0]), "ibag chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("igen")},[&](const ChunkHead& h,auto& is) {		// インストルメント(楽器)のジェネレータ情報
					igens.resize(h.size / sizeof(SFGen));
					read(is, igens.data(), igens.size() * sizeof(igens[0]), "igen chunk error");
				}},
				{{id("sfbk"),id("pdta"),id("shdr")},[&](const ChunkHead& h,auto& is) {		// サンプル(音声波形)のヘッダ情報
					doc.shdr.resize(h.size / sizeof(SFSampleHeader));
					read(is, doc.shdr.data(), doc.shdr.size() * sizeof(doc.shdr[0]), "shdr chunk error");
				}},
			};

			std::vector<std::array<int8_t, 4>> types;
			readRiff(
				is,
				[&](const ChunkList& info) {
					types.push_back(info.type);
				},
				[&] {
					types.pop_back();
				},
				[&](const ChunkHead& info, std::istream& is) {
					types.push_back(info.id);
					if (auto it = map.find(types); it != map.end()) {
						it->second(info, is);
					} else {
						throw std::runtime_error("unknown chunk error");	// 不明なチャンク
					}
					types.pop_back();
				});

			const auto gensToBag = [](const auto& bags, auto begin, auto end) {
				Gen b;
				const auto generator = std::make_shared<GeneratorMap>();
				b.generator = generator;
				for (auto i = begin; i < end; i++) {
					const auto& g = bags.at(i);
					(*generator)[static_cast<GenOperator>(g.genOper)] = g.genAmount;
				}
				return b;
			};
			const auto modToVector = [](const auto& mods, auto begin, auto end) {
				std::vector<SFMod> result(end - begin);
				for (auto i = begin; i < end; i++) {
					const auto& g = mods.at(i);
					auto& m = result[i - begin];
					m.srcOper = g.srcOper;
					m.destOper = static_cast<GenOperator>(g.destOper);
					m.modAmount = g.modAmount;
					m.amtSrcOper = g.amtSrcOper;
					m.modTransOper = g.modTransOper;
				}
				return result;
			};

			// 組み立て
			doc.presetHeaders.reserve(phdrs.size() - 1);
			for (auto i = 0; i < phdrs.size() - 1; i++) {
				const auto& c = phdrs[i];
				Doc::PresetHeader ph;
				*static_cast<SFPresetHeader*>(&ph) = c;

				// Bag
				for (auto bi = c.bagIndex; bi < phdrs[i + 1].bagIndex; bi++) {
					const auto& b = pbags.at(bi);
					Gen pb = gensToBag(pgens, b.genIndex, pbags.at(bi + 1).genIndex);
					pb.mods = modToVector(pmods, b.modIndex, pbags.at(bi + 1).modIndex);
					ph.bags.emplace_back(std::move(pb));
				}

				doc.presetHeaders.emplace_back(std::move(ph));
			}

			doc.instHeaders.reserve(insts.size()-1);
			for (auto i = 0; i < insts.size() - 1; i++) {
				const auto& c = insts[i];
				Doc::InstHeader ih;
				ih.name.resize(std::size(c.name));
				std::memcpy(ih.name.data(), c.name, std::size(c.name));
				ih.name.resize(std::strlen(ih.name.c_str()));			// \0 以降を削除

				// Bag
				for (auto bi = c.bagIndex; bi < insts[i + 1].bagIndex; bi++) {
					const auto& b = ibags.at(bi);
					Gen pb = gensToBag(igens, b.genIndex, ibags.at(bi + 1).genIndex);
					pb.mods = modToVector(imods, b.modIndex, ibags.at(bi + 1).modIndex);
					ih.bags.emplace_back(std::move(pb));
				}
				doc.instHeaders.emplace_back(std::move(ih));
			}

			return Parse(std::move(doc));
		}

	public:
		Doc		m_doc;		// 内部情報を壊すこと(所有権移動)を考慮して隠蔽しない
	private:
		Parse(Doc&& doc)
			:m_doc(std::move(doc))
		{}
	};

	// SoundFont を扱いやすい形式にしたもの
	class Soundfont {
	public:
		template <typename T = double> static std::variant<std::monostate, T, int16_t, std::pair<uint8_t, uint8_t>, enumSampleMode> getGenAmount(
			GenOperator ope, const GeneratorMap& instLocal, const GeneratorMap& instGlobal, const GeneratorMap& presetLocal, const GeneratorMap& presetGlobal
		) {
			const auto get = [&](const GeneratorMap& map, int16_t min, int16_t max)->std::optional<int16_t> {
				const auto it = map.find(ope);
				if (it == map.cend()) return std::optional<int16_t>();
				const auto n = it->second;
				if (n < min || n > max) {
					std::clog << "[info] out of range generator value. ope:" << static_cast<int>(ope) << " value:" << n << std::endl;
				}
				return std::clamp(n, min, max);
			};
			const auto getIntInst = [&](int16_t min, int16_t max, int16_t def) {
				auto r = get(instLocal, min, max);
				return r ? *r : get(instGlobal, min, max).value_or(def);
			};
			const auto getInt = [&](int16_t min, int16_t max, int16_t def)->int16_t {
				int n = getIntInst(min, max, def);
				auto pr = get(presetLocal, min, max);
				if (!pr) pr = get(presetGlobal, min, max);
				if (pr) n += *pr;
				return std::clamp<int>(n, min, max);
			};
			const auto getEnv = [&](int16_t max)->T {		// ○○Env のケース
				constexpr int16_t mindef = -12000;
				const auto n = getInt(mindef, max, mindef);
				if (n <= mindef) return static_cast<T>(0.0);		// 最小値なら 0 とする特別処理
				return std::pow(static_cast<T>(2.0), n / static_cast<T>(1200.0));
			};

			switch (static_cast<GenOperator>(ope)) {
			case GenOperator::startAddrsOffset:			return getIntInst(0, 32767, 0);			// only valid at the instrument level
			case GenOperator::endAddrsOffset:			return getIntInst(0, 32767, 0);			// only valid at the instrument level
			case GenOperator::startloopAddrsOffset:		return getIntInst(0, 32767, 0);			// only valid at the instrument level
			case GenOperator::endloopAddrsOffset:		return getIntInst(0, 32767, 0);			// only valid at the instrument level
			case GenOperator::startAddrsCoarseOffset:	return getIntInst(0, 32767, 0);			// only valid at the instrument level
			case GenOperator::modLfoToPitch:			return getInt(-12000, 12000, 0) / static_cast<T>(100.0);
			case GenOperator::vibLfoToPitch:			return getInt(-12000, 12000, 0) / static_cast<T>(100.0);
			case GenOperator::modEnvToPitch:			return getInt(-12000, 12000, 0) / static_cast<T>(100.0);
			case GenOperator::initialFilterFc:			return static_cast<T>(8.176) * std::pow(static_cast<T>(2.0), getInt(1500, 13500, 13500) / static_cast<T>(1200.0));
			case GenOperator::initialFilterQ:			return getInt(0, 960, 0) / static_cast<T>(10.0);
			case GenOperator::modLfoToFilterFc:			return getInt(-12000, 12000, 0) / static_cast<T>(100.0);
			case GenOperator::modEnvToFilterFc:			return getInt(-12000, 12000, 0) / static_cast<T>(100.0);
			case GenOperator::endAddrsCoarseOffset:		return getIntInst(0, 32767, 0);			// only valid at the instrument level
			case GenOperator::modLfoToVolume:			return getInt(-960, 960, 0) / static_cast<T>(10.0);
			case GenOperator::unused1:					assert(false);	return {};
			case GenOperator::chorusEffectsSend:		return getInt(0, 1000, 0) / static_cast<T>(10.0);
			case GenOperator::reverbEffectsSend:		return getInt(0, 1000, 0) / static_cast<T>(10.0);
			case GenOperator::pan:						return getInt(-500, 500, 0) / static_cast<T>(10.0);
			case GenOperator::unused2:					assert(false);	return {};
			case GenOperator::unused3:					assert(false);	return {};
			case GenOperator::unused4:					assert(false);	return {};
			case GenOperator::delayModLFO:				return getInt(-12000, 5000, -12000) / static_cast<T>(1200.0);
			case GenOperator::freqModLFO:				return static_cast<T>(8.176) * std::pow(static_cast<T>(2.0), getInt(-16000, 4500, 0) / static_cast<T>(1200.0));
			case GenOperator::delayVibLFO:				return std::pow(static_cast<T>(2.0), getInt(-12000, 5000, -12000) / static_cast<T>(1200.0));
			case GenOperator::freqVibLFO:				return static_cast<T>(8.176) * std::pow(static_cast<T>(2.0), getInt(-16000, 4500, 0) / static_cast<T>(1200.0));
			case GenOperator::delayModEnv:				return getEnv(5000);
			case GenOperator::attackModEnv:				return getEnv(8000);
			case GenOperator::holdModEnv:				return getEnv(5000);
			case GenOperator::decayModEnv:				return getEnv(8000);
			case GenOperator::sustainModEnv:			return getInt(0, 1000, 0) / static_cast<T>(10.0);
			case GenOperator::releaseModEnv:			return getEnv(8000);
			case GenOperator::keynumToModEnvHold:		return std::pow(static_cast<T>(2.0), getInt(-12000, 12000, 0) / static_cast<T>(100.0));
			case GenOperator::keynumToModEnvDecay:		return std::pow(static_cast<T>(2.0), getInt(-12000, 12000, 0) / static_cast<T>(100.0));
			case GenOperator::delayVolEnv:				return getEnv(5000);
			case GenOperator::attackVolEnv:				return getEnv(8000);
			case GenOperator::holdVolEnv:				return getEnv(5000);
			case GenOperator::decayVolEnv:				return getEnv(8000);
			case GenOperator::sustainVolEnv:			return getInt(0, 1440, 0) / static_cast<T>(10.0);
			case GenOperator::releaseVolEnv:			return getEnv(8000);
			case GenOperator::keynumToVolEnvHold:		return getInt(-1200, 1200, 0) / static_cast<T>(100.0);
			case GenOperator::keynumToVolEnvDecay:		return getInt(-1200, 1200, 0) / static_cast<T>(100.0);
			case GenOperator::instrument:				assert(false);	return {};
			case GenOperator::reserved1:				assert(false);	return {};
			case GenOperator::keyRange:											// non-real-time parameter
			case GenOperator::velRange:											// non-real-time parameter
			{
				const auto getRange = [&](const auto& local, const auto& global) {
					auto v = [&] {
						if (auto r = get(local, 0, 0x7f7f)) return *r;
						return get(global, 0, 0x7f7f).value_or(0x7f00);
					}();
					return std::pair<uint8_t, uint8_t>(
						std::clamp<int>(v & 0xff, 0, 127),
						std::clamp<int>(v >> 8, 0, 127));
				};
				const auto ins = getRange(instLocal, instGlobal);
				const auto pre = getRange(presetLocal, presetGlobal);
				return std::pair<uint8_t, uint8_t>{ (std::max)(ins.first, pre.first), (std::min)(ins.second, pre.second) };
			};
			case GenOperator::startloopAddrsCoarseOffset:	return getIntInst(0, 32767, 0);	// only valid at the instrument level
			case GenOperator::keynum:					assert(false);	return {};			// only valid at the instrument level & non-real-time parameter
			case GenOperator::velocity:					assert(false);	return {};			// only valid at the instrument level & non-real-time parameter
			case GenOperator::initialAttenuation:		return getInt(0, 1440, 0) / static_cast<T>(10.0);
			case GenOperator::reserved2:				return {};
			case GenOperator::endloopAddrsCoarseOffset:	return getIntInst(0, 32767, 0);		// only valid at the instrument level
			case GenOperator::coarseTune:				return getInt(-120, 120, 0);
			case GenOperator::fineTune:					return getInt(-99, 99, 0);
			case GenOperator::sampleID:					assert(false);	return {};
			case GenOperator::sampleModes:				return static_cast<enumSampleMode>(getIntInst(0, 3, 0));	// only valid at the instrument level & non-real-time parameter
			case GenOperator::reserved3:				assert(false);	return {};
			case GenOperator::scaleTuning:				return getInt(0, 1200, 100);		// non-real-time parameter
			case GenOperator::exclusiveClass:												// only valid at the instrument level & non-real-time parameter
			case GenOperator::overridingRootKey:											// only valid at the instrument level & non-real-time parameter
			{
				if (auto r = get(instLocal, 0, 127)) return *r;
				if (auto r = get(instGlobal, 0, 127)) return *r;
				return {};
			}
			case GenOperator::unused5:					assert(false);	return {};
			case GenOperator::endOper:					assert(false);	return {};
			default:									assert(false);
			}
			return {};
		};

		class SampleBody {
			friend class Soundfont;
		public:
#ifndef NDEBUG
			int16_t						sampleId = 0;
			std::string					name;
#endif
			std::pair<uint32_t, uint32_t>	point;							// 音声波形データの開始位置、終了位置(単位：サンプル)
			std::pair<uint32_t, uint32_t>	loop;							// ループ位置(開始位置、終了位置)
			uint32_t					sampleRate = 0;						// 音声波形データのサンプリングレート
			uint8_t						originalKey = 0;					// 音声波形データのオリジナルの音程 60の時、音声波形はC4(中央のド、261.62Hz)の音程で録音された波形であることを示す
			int8_t						pitchCorrection = 0;				// オリジナルの音程に対しての補正(単位cent)
//			SFSampleLink				type = SFSampleLink::monoSample;	// 音声波形データのタイプ
			
			// 波形データ(実数に変換後の波形データ)
			template <typename T = double> std::vector<T> createSample(const Soundfont& sf)const {
				const auto end = (std::max)(point.second, point.first + loop.second) + 1;
				std::vector<T> sample(end - point.first);
				const auto& smpl = sf.m_doc.smpl;
				std::transform(&smpl[point.first], &smpl[end], sample.begin(), [](auto& x) {
					constexpr T sampleMax = static_cast<T>(1.0) / (std::numeric_limits<typename std::remove_reference_t<decltype(smpl)>::value_type>::max)();	// 1.0 / 32767
					return x * sampleMax;			// x / 32767
				});
				return sample;
			}

		};

		struct InstrumentSample {
			std::pair<uint8_t, uint8_t>			keyRange = { 0,0x7f };	// マッピングするキー(ノートNo)の範囲
			std::pair<uint8_t, uint8_t>			velRange = { 0,0x7f };	// マッピングするベロシティの範囲
			std::shared_ptr<const GeneratorMap>	genInstLocal = std::make_shared<GeneratorMap>();
			std::shared_ptr<const SampleBody>	spSample = std::make_shared<SampleBody>();
		};
		struct Instrument {
#ifndef NDEBUG
			int16_t								instrumentNo = 0;
			std::string							name;
#endif
			std::shared_ptr<const GeneratorMap>	genInstGlobal = std::make_shared<GeneratorMap>();
			std::shared_ptr<const GeneratorMap>	genPresetLocal = std::make_shared<GeneratorMap>();
			std::vector<InstrumentSample>		samples;

			Instrument(){}
			Instrument(Instrument&&) = default;
			Instrument& operator=(Instrument&&) = default;
#ifdef SOUNDFONT_COPYABLE
#else
			Instrument(const Instrument&) = delete;
			Instrument& operator=(const Instrument&) = delete;
#endif
		};

		struct Preset {
			const std::pair<uint16_t, uint16_t>	presetNo;		// <bank,presetno>
			std::string							name;
			std::shared_ptr<const GeneratorMap>	genPresetGlobal = std::make_shared<GeneratorMap>();		// グローバルゾーン
			std::vector<Instrument>				instruments;

			struct Less {
				typedef void is_transparent;
				bool operator()(const Preset& a, const Preset& b)			const { return a.presetNo < b.presetNo; }
				bool operator()(const decltype(presetNo) a, const Preset& b)const { return a < b.presetNo; }
				bool operator()(const Preset& a, const decltype(presetNo) b)const { return a.presetNo < b; }
			};

			Preset(uint16_t bank, uint16_t presetno)
				:presetNo(std::pair(bank, presetno))
			{}
			Preset(Preset&&) = default;
			Preset& operator=(Preset&&) = delete;	 // とりあえず
#ifdef SOUNDFONT_COPYABLE
#else
			Preset(const Preset&) = delete;
			Preset& operator=(const Preset&) = delete;
#endif
		};

		struct InstrumentRefer {
			std::reference_wrapper<const Preset>			preset;
			std::reference_wrapper<const Instrument>		instrument;
			std::reference_wrapper<const InstrumentSample>	instrumentSample;
		};

		struct PresetKey {
			uint16_t	bank = 0;
			uint16_t	presetNo = 0;
			uint8_t		note = 0;
			uint8_t		velocity = 0;
		};
		std::vector<InstrumentRefer> getPreset(const PresetKey& presetKey) const noexcept {
			std::vector<InstrumentRefer> result;
			const auto itPreset = m_doc.presets.find({ presetKey.bank ,presetKey.presetNo });
			if (itPreset == m_doc.presets.end()) {
				return result;
			}
			for (auto& instrument : itPreset->instruments) {
				for (const InstrumentSample& sample : instrument.samples) {
					if (presetKey.note < sample.keyRange.first || presetKey.note > sample.keyRange.second) {
						continue;
					}
					if (presetKey.velocity < sample.velRange.first || presetKey.velocity > sample.velRange.second) {
						continue;
					}
					result.emplace_back(InstrumentRefer{ *itPreset, instrument, sample });
				}
			}
			return result;
		}

		static Soundfont fromStream(std::istream& is) {
			Parse sf = Parse::fromStream(is);

			std::set<Preset, typename Preset::Less>	presets;
			std::map<uint16_t,std::shared_ptr<const SampleBody>> mapSample;

			for(auto &ph : sf.m_doc.presetHeaders ){
				Preset preset(ph.bank, ph.presetno);
				preset.name = ph.getName();

				const auto getNo = [](const auto& map, const auto& key) {
					const auto i = map.find(key);
					return i != map.end() ? std::optional<int16_t>(i->second) : std::optional<int16_t>();
				};

				for (const auto& pbag : ph.bags) {		// 最初にグローバルゾーン
					if (const auto instrumentNo = getNo(*pbag.generator, GenOperator::instrument); !instrumentNo) {
						if (preset.genPresetGlobal->size() > 0) {			// failsafe 既にグローバルゾーンが存在している
							std::clog << "[info] multiple definitions preset global. : " << preset.name << std::endl;
							assert(false);
						}
						preset.genPresetGlobal = pbag.generator;
						// break; チェックを行うのでbreakしない
					}
				}
				for (const auto& pbag : ph.bags) {
					if (const auto instrumentNo = getNo(*pbag.generator, GenOperator::instrument)) {	// instrumentNo

						auto& ih = sf.m_doc.instHeaders.at(*instrumentNo);

						Instrument instrument;
#ifndef NDEBUG
						instrument.instrumentNo = *instrumentNo;
						instrument.name = ih.name;
#endif
						instrument.genPresetLocal = pbag.generator;

						for (const auto& ibag : ih.bags) {		// 最初にグローバルゾーン
							if (const auto sampleID = getNo(*ibag.generator, GenOperator::sampleID); !sampleID) {
								if (instrument.genInstGlobal->size() != 0) {			// failsafe 既にグローバルゾーンが存在している
									std::clog << "[info] multiple definitions instrument global. : " << ih.name << std::endl;
									assert(false);
								}
								instrument.genInstGlobal = ibag.generator;
								// break; チェックを行うのでbreakしない
							}
						}
						for (const auto& ibag : ih.bags) {
							const auto sampleID = getNo(*ibag.generator, GenOperator::sampleID);	// sampleID
							if (!sampleID) continue;

							InstrumentSample sample;
							sample.genInstLocal = ibag.generator;

							auto& spSample = mapSample[*sampleID];
							if (!spSample) {
								auto sp = std::make_shared<SampleBody>();

								auto& sh = sf.m_doc.shdr.at(*sampleID);
#ifndef NDEBUG
								sp->sampleId = *sampleID;
								sp->name = sh.getName();
#endif

								//sp->type = sh.sfSampleType;
								sp->originalKey = sh.byOriginalPitch;
								sp->pitchCorrection = sh.chPitchCorrection;

								// RTRACE("\n%1 originalKey:%d sampleRate:%d", sp->name, static_cast<int>(sp->originalKey), sh.sampleRate);

								sp->sampleRate = sh.dwSampleRate;
								sp->point = { sh.dwStart,sh.dwEnd };
								sp->loop = { sh.dwStartloop - sh.dwStart,sh.dwEndloop - sh.dwStart };

								// データチェック
								if (sp->point.first > sp->point.second || sp->point.second >= sf.m_doc.smpl.size()) {
									std::clog << "[warning] sample pointer failed. : " << ih.name << std::endl;
									sp->point.first = 0;
									sp->point.second = 1;
								}
								if (sp->loop.first > sp->loop.second || sp->loop.second >= sf.m_doc.smpl.size() || sh.dwStartloop < sh.dwStart) {
									std::clog << "[warning] sample loop failed. : " << ih.name << std::endl;
									sp->loop.first = 0;
									sp->loop.second = 1;
								}

								spSample = sp;
							}
							sample.spSample = spSample;

							// generatorResult算出
							const auto getAmount = [&](GenOperator ope) {
								return getGenAmount(ope, *sample.genInstLocal, *instrument.genInstGlobal, *instrument.genPresetLocal, *preset.genPresetGlobal);
							};

							//if (spSample->name == "P200 Piano D5(L)") {
							//	int a = 0;
							//}

							sample.keyRange = std::get<std::pair<uint8_t, uint8_t>>(getAmount(GenOperator::keyRange));
							sample.velRange = std::get<std::pair<uint8_t, uint8_t>>(getAmount(GenOperator::velRange));

							//有効範囲が存在しない場合は破棄
							if (sample.keyRange.first > sample.keyRange.second ||
								sample.velRange.first > sample.velRange.second)
							{
								continue;
							}

							instrument.samples.emplace_back(std::move(sample));
						}
						preset.instruments.emplace_back(std::move(instrument));
					}
				}

				if (const auto it = presets.emplace(std::move(preset)); !it.second) {
					std::clog << "[info] preset overwrite: " << it.first->name << std::endl;	// 重複チェック
				}
			}

			return Soundfont(std::move(sf.m_doc.info), std::move(sf.m_doc.smpl), std::move(presets));
		}

		Soundfont(Soundfont&&) = default;
		Soundfont& operator=(Soundfont&&) = default;

#ifdef SOUNDFONT_COPYABLE
		SoundfontT(const SoundfontT& s)
			:m_doc(s.m_doc)
		{}
		SoundfontT& operator=(const SoundfontT& s) {
			m_doc = s.doc();
			return *this;
		}
#else
		Soundfont(const Soundfont&) = delete;
		Soundfont& operator=(const Soundfont&) = delete;
#endif

	private:
		struct {
			FileInfo								fileInfo;
			std::vector<int16_t>					smpl;
			std::set<Preset, typename Preset::Less>	presets;
		}m_doc;

		Soundfont(FileInfo&& fileInfo, std::vector<int16_t>&& smpl, std::set<Preset, typename Preset::Less>&& presets)
			: m_doc{ std::move(fileInfo),std::move(smpl), std::move(presets) }
		{}
	public:
		const decltype(m_doc)& doc()const {
			return m_doc;
		}
	};



}