// miniaudio_impl.cpp: übersetzt die Bibliothek miniaudio (Header-only, Public Domain / MIT-0).
//
// Eigene Datei, weil miniaudio Warnungen auslöst, die für fremden Code nicht stören sollen; sie wird
// in der CMakeLists ohne unsere Warnoptionen gebaut. Wir brauchen nur die Aufnahme: Decoder, Encoder,
// Engine und Node-Graph bleiben draußen, das spart Übersetzungszeit und Größe.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#include "miniaudio.h"
