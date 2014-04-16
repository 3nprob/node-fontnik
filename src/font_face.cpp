/*****************************************************************************
 *
 * This file is part of Mapnik (c++ mapping toolkit)
 *
 * Copyright (C) 2013 Artem Pavlenko
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#include "font_face.hpp"
#include "distmap.h"

// stl
#include <iostream>

// icu
#include <unicode/unistr.h>

namespace fontserver {

font_face::font_face(FT_Face face)
    : face_(face),
      glyphs_(),
      char_height_(0.0) {}

font_face::~font_face() {
    /*
    std::cout << "font_face: Clean up face " << family_name()
        << " " << style_name() << "\n";

    FT_Done_Face(face_);
    */
}

double font_face::get_char_height() const {
    if (char_height_ != 0.0) return char_height_;

    glyph_info tmp;
    tmp.glyph_index = FT_Get_Char_Index(face_, 'X');
    glyph_dimensions(tmp);
    char_height_ = tmp.height;

    return char_height_;
}

bool font_face::set_character_sizes(double size) {
    char_height_ = 0.0;
    return !FT_Set_Char_Size(face_, 0, (FT_F26Dot6)(size * (1<<6)), 0, 0);
}

void font_face::glyph_dimensions(glyph_info &glyph) const {
    // Check if char is already in cache.
    std::map<uint32_t, glyph_info>::const_iterator itr;
    itr = glyphs_.find(glyph.glyph_index);
    if (itr != glyphs_.end()) {
        glyph = itr->second;
        return;
    }

    // TODO: remove hardcoded buffer size?
    int buffer = 3;

    // Transform with identity matrix and null vector.
    // TODO: Is this necessary?
    FT_Set_Transform(face_, 0, 0);

    FT_UInt char_index = FT_Get_Char_Index(face_, glyph.glyph_index);
    if (!char_index) return;

    if (FT_Load_Glyph(face_, char_index, FT_LOAD_NO_HINTING)) return;

    FT_GlyphSlot slot = face_->glyph;
    int width = slot->bitmap.width;
    int height = slot->bitmap.rows;

    FT_Glyph image;
    if (FT_Get_Glyph(slot, &image)) return;

    FT_BBox bbox;
    FT_Glyph_Get_CBox(image, FT_GLYPH_BBOX_PIXELS, &bbox);
    FT_Done_Glyph(image);

    glyph.width = width;
    glyph.height = height;
    glyph.left = slot->bitmap_left;
    glyph.top = slot->bitmap_top;

    glyph.ymin = bbox.yMin;
    glyph.ymax = bbox.yMax;
    glyph.line_height = face_->size->metrics.height / 64.0;
    glyph.advance = slot->metrics.horiAdvance / 64.0;

    // Create a signed distance field (SDF) for the glyph bitmap.
    if (width > 0) {
        unsigned int buffered_width = width + 2 * buffer;
        unsigned int buffered_height = height + 2 * buffer;

        unsigned char *distance = make_distance_map((unsigned char *)slot->bitmap.buffer, width, height, buffer);

        glyph.bitmap.resize(buffered_width * buffered_height);
        for (unsigned int y = 0; y < buffered_height; y++) {
            memcpy((unsigned char *)glyph.bitmap.data() + buffered_width * y, distance + y * distmap_size, buffered_width);
        }
        free(distance);
    }

    glyphs_.insert(std::pair<uint32_t, glyph_info>(glyph.glyph_index, glyph));
}

/*
void stroker::init(double radius) {
    FT_Stroker_Set(s_, (FT_Fixed) (radius * (1<<6)),
                   FT_STROKER_LINECAP_ROUND,
                   FT_STROKER_LINEJOIN_ROUND,
                   0);
}

stroker::~stroker() {
    MAPNIK_LOG_DEBUG(font_engine_freetype) << "stroker: Destroy stroker=" << s_;

    FT_Stroker_Done(s_);
}
*/

}
