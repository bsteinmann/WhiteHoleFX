// builtin_text_font.hpp: die Pixelschrift des Text-Nodes als WGSL-Prelude.
//
// Warum es diese Datei gibt: Der Text-Node zeichnet Buchstaben im Shader, ohne Textur. Die Schrift ist
// eine klassische 5x7-Punktmatrix (Spalte für Spalte, Bit 0 = oberste Zeile), je Zeichen fünf Bytes in
// zwei u32 gepackt: Spalten 0-3 im ersten Wort (Spalte 0 unten), Spalte 4 im zweiten. Enthalten sind
// ASCII 32-126 (Index 0-94) und die deutschen Sonderzeichen ä ö ü Ä Ö Ü ß (Index 95-101). Bei Kleinbuchstaben
// bleiben Zeile 0 und 1 frei (Platz für die Umlautpunkte); große Umlaute sind um eine Zeile nach unten
// gerückt und füllen so alle acht Zeilen der Zelle. Alles andere zeigt ein Fragezeichen.
//
// Das Prelude wird wie alle Preludes nur einmal je Graph in den Shader geschrieben (gleicher Text).
#pragma once

namespace whitehole::nodes::detail {

inline constexpr const char* kTextPrelude = R"wgsl(var<private> wh_font: array<u32, 204> = array<u32, 204>(
    0x00000000u, 0x00000000u, 0x005F0000u, 0x00000000u, 0x07000700u, 0x00000000u, 0x7F147F14u, 0x00000014u,
    0x2A7F2A24u, 0x00000012u, 0x64081323u, 0x00000062u, 0x22554936u, 0x00000050u, 0x00030500u, 0x00000000u,
    0x41221C00u, 0x00000000u, 0x1C224100u, 0x00000000u, 0x083E0814u, 0x00000014u, 0x083E0808u, 0x00000008u,
    0x00305000u, 0x00000000u, 0x08080808u, 0x00000008u, 0x00606000u, 0x00000000u, 0x04081020u, 0x00000002u,
    0x4549513Eu, 0x0000003Eu, 0x407F4200u, 0x00000000u, 0x49516142u, 0x00000046u, 0x4B454121u, 0x00000031u,
    0x7F121418u, 0x00000010u, 0x45454527u, 0x00000039u, 0x49494A3Cu, 0x00000030u, 0x05097101u, 0x00000003u,
    0x49494936u, 0x00000036u, 0x29494906u, 0x0000001Eu, 0x00363600u, 0x00000000u, 0x00365600u, 0x00000000u,
    0x41221408u, 0x00000000u, 0x14141414u, 0x00000014u, 0x14224100u, 0x00000008u, 0x09510102u, 0x00000006u,
    0x41794932u, 0x0000003Eu, 0x1111117Eu, 0x0000007Eu, 0x4949497Fu, 0x00000036u, 0x4141413Eu, 0x00000022u,
    0x2241417Fu, 0x0000001Cu, 0x4949497Fu, 0x00000041u, 0x0909097Fu, 0x00000001u, 0x4949413Eu, 0x0000007Au,
    0x0808087Fu, 0x0000007Fu, 0x417F4100u, 0x00000000u, 0x3F414020u, 0x00000001u, 0x2214087Fu, 0x00000041u,
    0x4040407Fu, 0x00000040u, 0x020C027Fu, 0x0000007Fu, 0x1008047Fu, 0x0000007Fu, 0x4141413Eu, 0x0000003Eu,
    0x0909097Fu, 0x00000006u, 0x2151413Eu, 0x0000005Eu, 0x2919097Fu, 0x00000046u, 0x49494946u, 0x00000031u,
    0x017F0101u, 0x00000001u, 0x4040403Fu, 0x0000003Fu, 0x2040201Fu, 0x0000001Fu, 0x4038403Fu, 0x0000003Fu,
    0x14081463u, 0x00000063u, 0x08700807u, 0x00000007u, 0x45495161u, 0x00000043u, 0x41417F00u, 0x00000000u,
    0x10080402u, 0x00000020u, 0x7F414100u, 0x00000000u, 0x02010204u, 0x00000004u, 0x40404040u, 0x00000040u,
    0x04020100u, 0x00000000u, 0x54545420u, 0x00000078u, 0x4444487Fu, 0x00000038u, 0x44444438u, 0x00000020u,
    0x48444438u, 0x0000007Fu, 0x54545438u, 0x00000018u, 0x01097E08u, 0x00000002u, 0x5252520Cu, 0x0000003Eu,
    0x0404087Fu, 0x00000078u, 0x407D4400u, 0x00000000u, 0x3D444020u, 0x00000000u, 0x4428107Fu, 0x00000000u,
    0x407F4100u, 0x00000000u, 0x0418047Cu, 0x00000078u, 0x0404087Cu, 0x00000078u, 0x44444438u, 0x00000038u,
    0x1414147Cu, 0x00000008u, 0x18141408u, 0x0000007Cu, 0x0404087Cu, 0x00000008u, 0x54545448u, 0x00000020u,
    0x40443F04u, 0x00000020u, 0x2040403Cu, 0x0000007Cu, 0x2040201Cu, 0x0000001Cu, 0x4030403Cu, 0x0000003Cu,
    0x28102844u, 0x00000044u, 0x5050500Cu, 0x0000003Cu, 0x4C546444u, 0x00000044u, 0x41360800u, 0x00000000u,
    0x007F0000u, 0x00000000u, 0x08364100u, 0x00000000u, 0x10080810u, 0x00000008u, 0x55545520u, 0x00000078u,
    0x45444538u, 0x00000038u, 0x2140413Cu, 0x0000007Cu, 0x212221FCu, 0x000000FCu, 0x8182817Cu, 0x0000007Cu,
    0x8180817Eu, 0x0000007Eu, 0x5649017Eu, 0x00000020u
);

fn wh_text_glyph(cp: u32) -> u32 {
    if (cp >= 32u && cp <= 126u) {
        return cp - 32u;
    }
    switch (cp) {
        case 0xE4u: { return 95u; }
        case 0xF6u: { return 96u; }
        case 0xFCu: { return 97u; }
        case 0xC4u: { return 98u; }
        case 0xD6u: { return 99u; }
        case 0xDCu: { return 100u; }
        case 0xDFu: { return 101u; }
        default: { return 31u; }
    }
}

fn wh_text_bit(glyph: u32, col: u32, row: u32) -> bool {
    var bits = wh_font[glyph * 2u + 1u];
    if (col < 4u) {
        bits = wh_font[glyph * 2u] >> (8u * col);
    }
    return ((bits >> row) & 1u) == 1u;
}
)wgsl";

}  // namespace whitehole::nodes::detail
