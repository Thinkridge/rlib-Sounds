"use client";
import React from "react";
import { getRlibMml } from "@thinkridge/rlib-mml";
import { RlibSoundfont } from "@thinkridge/rlib-soundfont";
import { AudioPlayer } from "./models/AudioPlayer";

export default function Home() {
  const [mml, setMml] = React.useState("cdefgab<c");
  const [soundfont, setSoundfont] = React.useState<{ instance: RlibSoundfont; filename: string }>();
  const refAudioPlayer = React.useRef<AudioPlayer>(null);
  const [playState, setPlayState] = React.useState("");

  const donwload = (p: { blob: Blob; filename: string }) => {
    const url = URL.createObjectURL(p.blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = p.filename;
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  };

  return (
    <>
      <div
        style={{
          display: "flex",
          margin: "10px",
        }}
      >
        <button
          onClick={async () => {
            const input = document.createElement("input"); // ファイル選択ダイアログ
            input.type = "file";
            input.accept = ".sf2";
            input.onchange = async (ev: Event) => {
              try {
                if (!input.files) return;
                const file = input.files[0];
                const uint8Array = new Uint8Array(await file.arrayBuffer());

                const rlibSoundfont = new RlibSoundfont();
                await rlibSoundfont.initSoundfont(uint8Array);
                setSoundfont((before) => {
                  before?.instance.dispose(); // rlibSoundfont を破棄
                  return { instance: rlibSoundfont, filename: file.name };
                });
              } catch (e) {
                window.alert(e);
                console.error(e);
              } finally {
                input.onchange = null;
              }
            };
            input.click(); // ファイル選択ダイアログを開く
          }}
        >
          load SoundFont
        </button>
        <div
          style={{
            display: "flex",
            alignItems: "center",
          }}
        >
          {soundfont?.filename}
        </div>
        {soundfont && (
          <button
            onClick={async () => {
              try {
                const info = await soundfont.instance.info();
                if (info) {
                  const stringify = JSON.stringify(info, null, 2);
                  console.log(stringify);
                  window.alert(stringify);
                }
              } catch (e) {
                window.alert(e);
                console.error(e);
              }
            }}
          >
            info
          </button>
        )}
      </div>

      <div
        style={{
          margin: "10px",
          display: "flex",
        }}
      >
        <textarea
          value={mml}
          onChange={(e) => setMml(e.target.value)}
          rows={12}
          style={{
            minWidth: "400px",
            fontFamily: "monospace",
            fontSize: "14px",
            whiteSpace: "pre",
            boxSizing: "border-box",
          }}
          placeholder="MML area" //
        />
      </div>
      <div
        style={{
          margin: "10px",
          display: "flex",
        }}
      >
        <button
          disabled={mml.trim().length <= 0 || !soundfont}
          onClick={async () => {
            try {
              if (!soundfont) return;
              const rlibMml = await getRlibMml();
              const r = rlibMml.mmlToSmf(mml);
              if (!r.smf) throw new Error(JSON.stringify(r.errors));
              const wav = await soundfont.instance.smfToWav(r.smf);
              const blob = new Blob([wav as BlobPart], { type: "audio/wav" });
              if (!refAudioPlayer.current) {
                refAudioPlayer.current = new AudioPlayer(new AudioContext());
              }
              const ap = refAudioPlayer.current;
              ap.setEnded((e) => {
                ap.pause();
                ap.setPosition(0);
              });
              ap.setOnChangedState((ev) => {
                setPlayState(ev);
              });
              await ap.open(await blob.arrayBuffer());
              ap.play();
            } catch (e) {
              window.alert(e);
              console.error(e);
            }
          }}
        >
          play
        </button>
        {playState && (
          <button
            onClick={async () => {
              if (!refAudioPlayer.current) return;
              try {
                const ap = refAudioPlayer.current;
                if (playState === "play") {
                  ap.pause();
                } else {
                  ap.play();
                }
              } catch (e) {
                window.alert(e);
                console.error(e);
              }
            }}
          >
            {playState === "play" ? "pause" : "unpause"}
          </button>
        )}
      </div>
      <div
        style={{
          margin: "10px",
          display: "flex",
        }}
      >
        <button
          onClick={async () => {
            const input = document.createElement("input"); // ファイル選択ダイアログ
            input.type = "file";
            input.accept = "audio/midi";
            input.onchange = async (ev: Event) => {
              try {
                if (!input.files) return;
                const file = input.files[0];
                const uint8Array = new Uint8Array(await file.arrayBuffer());
                const rlibMml = await getRlibMml();
                const mml = rlibMml.smfToMml(uint8Array);
                setMml(mml);
              } catch (e) {
                window.alert(e);
                console.error(e);
              } finally {
                input.onchange = null;
              }
            };
            input.click();
          }}
        >
          import smf
        </button>

        <button
          disabled={mml.trim().length <= 0}
          onClick={async () => {
            try {
              const rlibMml = await getRlibMml();
              const r = rlibMml.mmlToSmf(mml);
              if (!r.smf) throw new Error(JSON.stringify(r.errors));
              const blob = new Blob([r.smf as BlobPart], { type: "audio/midi" });
              donwload({ blob, filename: "mmlToSmf.mid" });
            } catch (e) {
              window.alert(e);
              console.error(e);
            }
          }}
        >
          export smf
        </button>

        <button
          disabled={mml.trim().length <= 0 || !soundfont}
          onClick={async () => {
            try {
              if (!soundfont) return;
              const rlibMml = await getRlibMml();
              const r = rlibMml.mmlToSmf(mml);
              if (!r.smf) throw new Error(JSON.stringify(r.errors));
              const wav = await soundfont.instance.smfToWav(r.smf);
              const blob = new Blob([wav as BlobPart], { type: "audio/wav" });
              donwload({ blob, filename: "mmlToWav.wav" });
            } catch (e) {
              window.alert(e);
              console.error(e);
            }
          }}
        >
          export wav
        </button>
      </div>
    </>
  );
}
