#pragma once
#include "util.h"
#include "source.h"

namespace video {
    // return (timestamp, path to .wav) list
    vec<float> create(VideoWriter &out, VideoCapture &src, vec<Evt> evts, vec<int> srccut);
    Mat write_evt(VideoCapture &src, const vec<int> &active, int frm, const vec<Evt> &evts, vec<float> &aud);
}
