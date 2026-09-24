// ymfm の ymfm_opn.cpp をこのプロジェクト側でラップするファイル。
//
// FmRenderer.h の ym2203fm は ymfm::fm_engine_base<ymfm::opn_registers> を直接使う。
// このテンプレートのメンバ実装は ymfm_opn.cpp 内でしか見えず、他の翻訳単位からリンクすると undefined symbol になる。
// そこで ymfm_opn.cpp をそのまま取り込み、必要なテンプレートを明示的にインスタンス化して実体を出力する。
//
// ビルド対象には ymfm/ymfm_opn.cpp の代わりにこのファイルを指定すること(二重定義になるため両方は指定しない)。
#include "../ymfm/ymfm_opn.cpp"

namespace ymfm
{
template class fm_engine_base<opn_registers>;
template class fm_engine_base<opna_registers>;
}
