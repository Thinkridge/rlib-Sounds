export class AudioPlayer {
  readonly context: AudioContext;
  opened?: // openで構築 closeで破棄
  {
    buffer: AudioBuffer;
    play?: // playで構築 pauseで破棄
    {
      source: AudioBufferSourceNode;
      startedAt: number;
    };
    pausedPosition: number;
  };
  endedCallback?: (ev: Event) => void;
  callbackOnChangedState?: (ev: ReturnType<AudioPlayer["getState"]>) => void;

  constructor(audioContext: AudioContext) {
    this.context = audioContext;
  }

  close() {
    this.pauseCore();
    this.opened = undefined;
    // 通知
    this.callbackOnChangedState?.(this.getState());
  }

  async open(buffer: ArrayBuffer) {
    const buf = await this.context.decodeAudioData(buffer);
    this.close();
    this.opened = {
      buffer: buf,
      pausedPosition: 0,
    };
    // 通知
    this.callbackOnChangedState?.(this.getState());
  }

  setEnded(callback?: (ev: Event) => void) {
    if (this.opened && this.opened.play && this.endedCallback) {
      this.opened.play.source.removeEventListener("ended", this.endedCallback);
    }
    this.endedCallback = callback;
    if (this.opened && this.opened.play && this.endedCallback) {
      this.opened.play.source.addEventListener("ended", this.endedCallback);
    }
  }

  setOnChangedState(callback?: AudioPlayer['callbackOnChangedState']) {
    this.callbackOnChangedState = callback;
  }

  getState() {
    if (!this.opened) return "close";
    if (this.opened.play) return "play";
    return "pause";
  }

  play() {
    if (!this.opened || this.opened.play) return;
    this.opened.play = {
      source: this.context.createBufferSource(),
      startedAt: this.context.currentTime - this.opened.pausedPosition,
    };
    this.opened.play.source.buffer = this.opened.buffer;
    this.opened.play.source.connect(this.context.destination);
    if (this.endedCallback) {
      this.opened.play.source.addEventListener("ended", this.endedCallback);
    }
    this.opened.play.source.start(0, this.opened.pausedPosition);

    // 通知
    this.callbackOnChangedState?.(this.getState());
  }

  pause() {
    this.pauseCore();
    // 通知
    this.callbackOnChangedState?.(this.getState());
  }

  private pauseCore() {
    if (!this.opened || !this.opened.play) {
      return;
    }
    this.opened.pausedPosition = this.getPosition();
    if (this.endedCallback) {
      this.opened.play.source.removeEventListener("ended", this.endedCallback);
    }

    this.opened.play.source.stop(0);
    this.opened.play.source.disconnect();

    this.opened.play = undefined;
  }

  // 再生中？(一時停止中はfalse)
  isPlaying() {
    return this.opened ? !!this.opened.play : false;
  }

  getDuration() {
    return this.opened ? this.opened.buffer.duration : 0;
  }

  setPosition(pos: number) {
    if (!this.opened) return;
    if (this.opened.play) {
      if (pos > this.getDuration()) {
        pos = this.getDuration();
      }
      this.pause();
      this.opened.pausedPosition = pos;
      this.play();
    } else {
      this.opened.pausedPosition = pos;
    }
  }

  getPosition() {
    if (!this.opened) return 0;
    if (!this.opened.play) return this.opened.pausedPosition;
    return this.context.currentTime - this.opened.play.startedAt;
  }

  getOutputTimestamp() {
    return this.context.getOutputTimestamp();
  }
}
