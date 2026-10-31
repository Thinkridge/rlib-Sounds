/** 小節上の位置 */
export type Location = {
  measure: number; // 小節( 0 ～ )
  beat: number; // 拍( 0 <= x < numerator )
  tick: number; // tick( 0 <= x < ticksPerBeat )
};

/** 拍子の区間 */
export type MeterSection = {
  tick: number; // 区間の開始位置(tick)
  measure: number; // 区間の開始小節( 0 ～ )
  numerator: number; // 分子
  denominator: number; // 分母(4 = 四分音符, 8 = 八分音符)
  ticksPerBeat: number; // 1拍の tick 数
};

/**
 * SMF の 位置(tick) / 小節位置(Location) / 時間(秒) の相互変換。
 *
 * - tick の分解能は 4分音符あたり 480 固定(SMF が別の分解能でも 480 に換算される)。
 * - 曲の終端が存在しない(最後の拍子・テンポがそのまま続く)ものとして変換する。
 * - 負の値は 0 として扱う。
 * - 時間 -> tick は切り捨て。tick/Location -> 時間 -> tick/Location は一致する。
 * - 不要になったら dispose() で解放する。呼び忘れても GC 時に自動で解放される。
 */
export interface Timeline {
  /** tick から位置(Location) */
  getLocationFromTick(tick: number): Location;
  /** 位置(Location)から tick */
  getTickFromLocation(loc: Location): number;
  /** 時間(曲の先頭からの秒)から位置(Location) */
  getLocationFromTime(sec: number): Location;
  /** 位置(Location)から時間(秒) */
  getTimeFromLocation(loc: Location): number;
  /** tick から時間(秒) */
  getTimeFromTick(tick: number): number;
  /** 時間(秒)から tick(端数は切り捨て) */
  getTickFromTime(sec: number): number;
  /** 各小節の開始 tick。endTick を含む小節の次の小節の開始位置(最後の小節線)まで */
  getMeasureTicks(endTick: number): Uint32Array;
  /** 拍子の区間(tick 昇順。先頭は必ず tick=0、拍子が無ければ 4/4) */
  getSections(): MeterSection[];
  /** 解放する。以降は使えない */
  dispose(): void;
}

/** Track のイベント。"type" で種類が分かれる。位置(tick)は 4分音符あたり 480 */
export type TrackEvent =
  | { type: "note"; tick: number; note: number; length: number; velocity: number } // NoteOn と NoteOff を1つにまとめたもの。length は音長(tick)
  | { type: "programChange"; tick: number; program: number }
  | { type: "controlChange"; tick: number; controller: number; value: number }
  | { type: "pitchBend"; tick: number; value: number } // -8192 ～ 8191
  | { type: "tempo"; tick: number; bpm: number }
  | { type: "timeSignature"; tick: number; numerator: number; denominator: number; clock: number; n32: number }
  | { type: "meta"; tick: number; metaType: number; data: number[] } // 上以外のメタイベント(テキスト、End of Track など)
  | { type: "sysEx"; tick: number; data: number[] }; // 先頭のバイトは 0xf0 / 0xf7

/** チャンネル+ポートごとのトラック。events は tick 昇順(同じ tick は SMF 内の順) */
export type Track = {
  name: string; // トラック名
  instrument: string; // 楽器名
  channel: number; // チャンネル 0～15
  port?: number; // MIDI Port 0～255。指定がなければ無し
  events: TrackEvent[];
};

/** SMF をトラックにしたもの */
export type TracksData = {
  ticksPerQuarter: number; // 4分音符あたりの tick 数。480 に換算した値
  endTick: number; // 全トラックで最後のイベントの位置(ノートは終了位置)
  tracks: Track[];
};

const timelineRegistry: FinalizationRegistry<{ native: any; deleted: boolean }> | undefined =
  typeof FinalizationRegistry !== "undefined"
    ? new FinalizationRegistry((h) => {
        if (!h.deleted) {
          h.deleted = true;
          try {
            h.native.delete();
          } catch (e) {}
        }
      })
    : undefined;

const wrapTimeline = (native: any): Timeline => {
  const holder = { native, deleted: false }; // ラッパー自身を参照しないこと(GC されなくなる)
  const n = () => {
    if (holder.deleted) throw new Error("Timeline is already disposed");
    return holder.native;
  };
  const mt: Timeline = {
    getLocationFromTick: (tick) => n().getLocationFromTick(tick),
    getTickFromLocation: (loc) => n().getTickFromLocation(loc),
    getLocationFromTime: (sec) => n().getLocationFromTime(sec),
    getTimeFromLocation: (loc) => n().getTimeFromLocation(loc),
    getTimeFromTick: (tick) => n().getTimeFromTick(tick),
    getTickFromTime: (sec) => n().getTickFromTime(sec),
    getMeasureTicks: (endTick) => n().getMeasureTicks(endTick),
    getSections: () => n().getSections(),
    dispose: () => {
      if (holder.deleted) return;
      holder.deleted = true;
      timelineRegistry?.unregister(mt);
      holder.native.delete();
    },
  };
  timelineRegistry?.register(mt, holder, mt);
  return mt;
};


let promiseRlibMml: Promise<any>;
export const getRlibMml = async () => {
  if (!promiseRlibMml) {
    const hModule = require("../../rlib-MML/wasm/build/rlibmml.js");
    promiseRlibMml = hModule();
  }
  const instance = await promiseRlibMml;

  return {
    mmlToSmf: (mml: string) => {
      const r = instance.mmlToSmf(mml) as {
        result: Uint8Array;
        error: undefined;
      } | {
        result: undefined;
        error: string;
      };
      if (r.result) {
        return { smf: r.result, errors: undefined };
      }
      try {
        return {
          smf: undefined,
          errors: (JSON.parse(r.error) as any)["errors"] as {
            code: number;
            line: number;
            column: number;
            message: string;
          }[],
        }
      } catch (e) {
        throw new Error(r.error);
      }
    },
    /**
     * SMF をトラック(チャンネル+ポートごと)の一覧にして取り出す。
     * NoteOn と NoteOff は音長付きの1つのノートになる。NoteOff がないノートは、次の同じ音の NoteOn まで(ただし8分音符が上限)。
     * ポリフォニックキープレッシャー・チャンネルプレッシャーは含まれない。
     */
    getTracks: (smf: Uint8Array): TracksData => {
      const r = instance.getTracks(smf) as { result: string; error: undefined } | { result: undefined; error: string };
      if (r.result === undefined) throw new Error(r.error);
      return JSON.parse(r.result) as TracksData;
    },
    /** SMF から Timeline(tick / 小節位置 / 時間の変換)を作る。不要になったら dispose() すること */
    getTimeline: (smf: Uint8Array): Timeline => {
      const r = instance.Timeline.create(smf) as
        | { result: any; error: undefined }
        | { result: undefined; error: string };
      if (!r.result) throw new Error(r.error);
      return wrapTimeline(r.result);
    },
    smfToMml: (smf: Uint8Array) => {
      const r = instance.smfToMml(smf) as {
        result: string;
        message?: undefined;
      } | {
        result: undefined;
        message: string;
      };
      if (!r.result) throw new Error(r.message);
      return r.result;
    },
  };
};

