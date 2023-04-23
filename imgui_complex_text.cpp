#include "imgui_internal.h"
// using namespace ComplexText;

void ImFontAtlas::EnableComplexTextLayout()
{
    ComplexText::raqm_complex = true;
}
void ImFontAtlas::DisableComplexTextLayout()
{
    ComplexText::raqm_complex = false;
}

const ImFontGlyph* ImFont::FindGlyphRaqm(int raqmIndex) const
{
    if (raqmIndex >= (size_t)IndexRaqmLookup.Size)
        return FallbackGlyph;
    const ImWchar i = IndexRaqmLookup.Data[raqmIndex];
    if (i == (ImWchar)-1)
        return FallbackGlyph;
    return &Glyphs.Data[i];
}

int utf8_encode(char *out, uint32_t utf)
{
  if (utf <= 0x7F) {
    // Plain ASCII
    out[0] = (char) utf;
    out[1] = 0;
    return 1;
  }
  else if (utf <= 0x07FF) {
    // 2-byte unicode
    out[0] = (char) (((utf >> 6) & 0x1F) | 0xC0);
    out[1] = (char) (((utf >> 0) & 0x3F) | 0x80);
    out[2] = 0;
    return 2;
  }
  else if (utf <= 0xFFFF) {
    // 3-byte unicode
    out[0] = (char) (((utf >> 12) & 0x0F) | 0xE0);
    out[1] = (char) (((utf >>  6) & 0x3F) | 0x80);
    out[2] = (char) (((utf >>  0) & 0x3F) | 0x80);
    out[3] = 0;
    return 3;
  }
  else if (utf <= 0x10FFFF) {
    // 4-byte unicode
    out[0] = (char) (((utf >> 18) & 0x07) | 0xF0);
    out[1] = (char) (((utf >> 12) & 0x3F) | 0x80);
    out[2] = (char) (((utf >>  6) & 0x3F) | 0x80);
    out[3] = (char) (((utf >>  0) & 0x3F) | 0x80);
    out[4] = 0;
    return 4;
  }
  else { 
    // error - use replacement character
    out[0] = (char) 0xEF;  
    out[1] = (char) 0xBF;
    out[2] = (char) 0xBD;
    out[3] = 0;
    return 0;
  }
}

void ImFont::BuildRaqmLookupTable()
{
    using namespace ComplexText;
    printf("Start Raqm Lookup. Glyph size:%d\n", Glyphs.Size);
    int max_codepoint = 0;
    int max_raqm_codepoint = 0;
    for (int j = 0; j != Glyphs.Size; j++)
        max_codepoint = ImMax(max_codepoint, (int)Glyphs[j].Codepoint);

    // Build lookup table
    IM_ASSERT(Glyphs.Size < 0xFFFF); // -1 is reserved
    IndexRaqmLookup.clear();
    memset(Used4kPagesMap, 0, sizeof(Used4kPagesMap));

    size_t q_count;
    raqm_direction_t dir = RAQM_DIRECTION_DEFAULT;
    raqm_glyph_t *qglyphs;
    int _raqmLookup[Glyphs.Size];

    if (!raqm_set_invisible_glyph(raqm_buf, -1))
    {
        int tmp = 0;
    }
    for (int i = 0; i < Glyphs.Size; i++)
    {
        int codepoint = (int)Glyphs[i].Codepoint;
        raqm_clear_contents(raqm_buf);

        if (raqm_buf != NULL)
        {
            char unicode_char[4];
            int mystrlength = utf8_encode(unicode_char, codepoint);

            if (raqm_set_text_utf8(raqm_buf, unicode_char, mystrlength) &&
                raqm_set_freetype_face(raqm_buf, face) &&
                raqm_set_par_direction(raqm_buf, dir) &&
                raqm_set_language(raqm_buf, "fa", 0, mystrlength) &&
                raqm_layout(raqm_buf))
            {
                qglyphs = raqm_get_glyphs(raqm_buf, &q_count);
                if(0<q_count){
                    if(max_raqm_codepoint<qglyphs[q_count-1].index)
                    {
                        max_raqm_codepoint = qglyphs[q_count-1].index;
                    }
                    _raqmLookup[i] = qglyphs[q_count-1].index;
                }
                else{
                    _raqmLookup[i] = -1;
                }
                // printf("%d- '%s': qindex: %d, codepoint: %d\n",i, unicode_char, qglyphs[0].index, codepoint);
            }
        }
    }
    printf("\n");
    if (max_raqm_codepoint + 1 > IndexRaqmLookup.Size)
        IndexRaqmLookup.resize(max_raqm_codepoint + 1, -1.0f);
    
    for (size_t i = 0; i < Glyphs.Size; i++)
    {
        if (0 < _raqmLookup[ i])
        {
            if(IndexRaqmLookup[_raqmLookup[ i]] <1)//_raqmLookup[2 * i + 1];
                IndexRaqmLookup[_raqmLookup[ i]] = (ImWchar)i;//_raqmLookup[2 * i + 1];
        }
    }
}

void ImFont::RenderGlyphs(ImDrawList* draw_list, float size, const ImVec2& pos, ImU32 col, const ImVec4& clip_rect, raqm_glyph_t *qglyphs, size_t q_count, float wrap_width, bool cpu_fine_clip) const
{
    // if (!text_end)
    //     text_end = text_begin + strlen(text_begin); // ImGui:: functions generally already provides a valid text_end, so this is merely to handle direct calls.

    // Align to be pixel perfect
    float x = IM_FLOOR(pos.x);
    float y = IM_FLOOR(pos.y);
    if (y > clip_rect.w)
        return;

    const float start_x = x;
    const float scale = size / FontSize;
    const float line_height = FontSize * scale;
    const bool word_wrap_enabled = (wrap_width > 0.0f);

    // Fast-forward to first visible line
    // const char* s = text_begin;
    // if (y + line_height < clip_rect.y)
    //     while (y + line_height < clip_rect.y && s < text_end)
    //     {
    //         const char* line_end = (const char*)memchr(s, '\n', text_end - s);
    //         if (word_wrap_enabled)
    //         {
    //             // FIXME-OPT: This is not optimal as do first do a search for \n before calling CalcWordWrapPositionA().
    //             // If the specs for CalcWordWrapPositionA() were reworked to optionally return on \n we could combine both.
    //             // However it is still better than nothing performing the fast-forward!
    //             s = CalcWordWrapPositionA(scale, s, line_end ? line_end : text_end, wrap_width);
    //             s = CalcWordWrapNextLineStartA(s, text_end);
    //         }
    //         else
    //         {
    //             s = line_end ? line_end + 1 : text_end;
    //         }
    //         y += line_height;
    //     }
    // For large text, scan for the last visible line in order to avoid over-reserving in the call to PrimReserve()
    // Note that very large horizontal line will still be affected by the issue (e.g. a one megabyte string buffer without a newline will likely crash atm)
    // if (text_end - s > 10000 && !word_wrap_enabled)
    // {
    //     const char* s_end = s;
    //     float y_end = y;
    //     while (y_end < clip_rect.w && s_end < text_end)
    //     {
    //         s_end = (const char*)memchr(s_end, '\n', text_end - s_end);
    //         s_end = s_end ? s_end + 1 : text_end;
    //         y_end += line_height;
    //     }
    //     text_end = s_end;
    // }
    // if (s == text_end)
    //     return;

    // Reserve vertices for remaining worse case (over-reserving is useful and easily amortized)
    const int vtx_count_max = (int)(q_count/*text_end - s*/) * 4;
    const int idx_count_max = (int)(q_count/*text_end - s*/) * 6;
    const int idx_expected_size = draw_list->IdxBuffer.Size + idx_count_max;
    draw_list->PrimReserve(idx_count_max, vtx_count_max);
    ImDrawVert*  vtx_write = draw_list->_VtxWritePtr;
    ImDrawIdx*   idx_write = draw_list->_IdxWritePtr;
    unsigned int vtx_index = draw_list->_VtxCurrentIdx;

    const ImU32 col_untinted = col | ~IM_COL32_A_MASK;
    const char* word_wrap_eol = NULL;

    for (size_t i = 0; i < q_count; i++)
    {
        int qindex = qglyphs[i].index;
        const ImFontGlyph *qglyph = FindGlyphRaqm(qindex);
        if (qglyph == NULL)
        {
            continue;
        }
        float char_width = qglyph->AdvanceX * scale;
        if (qglyph->Visible)
        {
            // We don't do a second finer clipping test on the Y axis as we've already skipped anything before clip_rect.y and exit once we pass clip_rect.w
            float x1 = x + qglyph->X0 * scale;
            float x2 = x + qglyph->X1 * scale;
            float y1 = y + qglyph->Y0 * scale;
            float y2 = y + qglyph->Y1 * scale;
            if (x1 <= clip_rect.z && x2 >= clip_rect.x)
            {
                // Render a character
                float u1 = qglyph->U0;
                float v1 = qglyph->V0;
                float u2 = qglyph->U1;
                float v2 = qglyph->V1;

                // CPU side clipping used to fit text in their frame when the frame is too small. Only does clipping for axis aligned quads.
                if (cpu_fine_clip)
                {
                    if (x1 < clip_rect.x)
                    {
                        u1 = u1 + (1.0f - (x2 - clip_rect.x) / (x2 - x1)) * (u2 - u1);
                        x1 = clip_rect.x;
                    }
                    if (y1 < clip_rect.y)
                    {
                        v1 = v1 + (1.0f - (y2 - clip_rect.y) / (y2 - y1)) * (v2 - v1);
                        y1 = clip_rect.y;
                    }
                    if (x2 > clip_rect.z)
                    {
                        u2 = u1 + ((clip_rect.z - x1) / (x2 - x1)) * (u2 - u1);
                        x2 = clip_rect.z;
                    }
                    if (y2 > clip_rect.w)
                    {
                        v2 = v1 + ((clip_rect.w - y1) / (y2 - y1)) * (v2 - v1);
                        y2 = clip_rect.w;
                    }
                    if (y1 >= y2)
                    {
                        x += char_width;
                        continue;
                    }
                }

                // Support for untinted glyphs
                ImU32 glyph_col = qglyph->Colored ? col_untinted : col;

                // We are NOT calling PrimRectUV() here because non-inlined causes too much overhead in a debug builds. Inlined here:
                {
                    vtx_write[0].pos.x = x1;
                    vtx_write[0].pos.y = y1;
                    vtx_write[0].col = glyph_col;
                    vtx_write[0].uv.x = u1;
                    vtx_write[0].uv.y = v1;
                    vtx_write[1].pos.x = x2;
                    vtx_write[1].pos.y = y1;
                    vtx_write[1].col = glyph_col;
                    vtx_write[1].uv.x = u2;
                    vtx_write[1].uv.y = v1;
                    vtx_write[2].pos.x = x2;
                    vtx_write[2].pos.y = y2;
                    vtx_write[2].col = glyph_col;
                    vtx_write[2].uv.x = u2;
                    vtx_write[2].uv.y = v2;
                    vtx_write[3].pos.x = x1;
                    vtx_write[3].pos.y = y2;
                    vtx_write[3].col = glyph_col;
                    vtx_write[3].uv.x = u1;
                    vtx_write[3].uv.y = v2;
                    idx_write[0] = (ImDrawIdx)(vtx_index);
                    idx_write[1] = (ImDrawIdx)(vtx_index + 1);
                    idx_write[2] = (ImDrawIdx)(vtx_index + 2);
                    idx_write[3] = (ImDrawIdx)(vtx_index);
                    idx_write[4] = (ImDrawIdx)(vtx_index + 2);
                    idx_write[5] = (ImDrawIdx)(vtx_index + 3);
                    vtx_write += 4;
                    vtx_index += 4;
                    idx_write += 6;
                }
            }
        }
        x += char_width;
    }


    // Give back unused vertices (clipped ones, blanks) ~ this is essentially a PrimUnreserve() action.
    draw_list->VtxBuffer.Size = (int)(vtx_write - draw_list->VtxBuffer.Data); // Same as calling shrink()
    draw_list->IdxBuffer.Size = (int)(idx_write - draw_list->IdxBuffer.Data);
    draw_list->CmdBuffer[draw_list->CmdBuffer.Size - 1].ElemCount -= (idx_expected_size - draw_list->IdxBuffer.Size);
    draw_list->_VtxWritePtr = vtx_write;
    draw_list->_IdxWritePtr = idx_write;
    draw_list->_VtxCurrentIdx = vtx_index;
}

std::string Text_to_ComplexUnicode( const char* text_begin, const char* text_end, int* out_text_length)
{
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;

    using namespace ComplexText;
    if (!text_end)
        text_end = text_begin + strlen(text_begin); // ImGui:: functions generally already provides a valid text_end, so this is merely to handle direct calls.

    raqm_direction_t dir = RAQM_DIRECTION_DEFAULT;
    const char *line_begin = text_begin;

    const char* s = text_begin;
    std::string finalText;

    while (s < text_end)
    {
        unsigned int c = (unsigned int)*s;
        if (c < 0x80)
            s += 1;
        else
            s += ImTextCharFromUtf8(&c, s, text_end);

        // if (c < 32)
        {
            if (c == '\n')
            {
                if (raqm_complex && library != NULL && face != NULL && raqm_buf != NULL)
                {
                    raqm_clear_contents(raqm_buf);
                    size_t q_count;
                    raqm_glyph_t *qglyphs;

                    if (raqm_buf != NULL)
                    {
                        int mystrlength = s - line_begin;

                        if (raqm_set_text_utf8(raqm_buf, line_begin, mystrlength) &&
                            raqm_set_freetype_face(raqm_buf, face) &&
                            raqm_set_par_direction(raqm_buf, dir) &&
                            raqm_set_language(raqm_buf, "fa", 0, mystrlength) &&
                            raqm_layout(raqm_buf))
                        {
                            qglyphs = raqm_get_glyphs(raqm_buf, &q_count);
                            for (size_t i = 0; i < q_count; i++)
                            {
                                const ImFontGlyph *qglyph = g.Font->FindGlyphRaqm(qglyphs[i].index);
                                char mchar[4];
                                int charL = utf8_encode(mchar, qglyph->Codepoint);
                                finalText.append(mchar);
                            }
                            finalText.append("\n");

                            // RenderGlyphs(draw_list, size, my_pos, col, clip_rect, qglyphs, q_count, wrap_width, cpu_fine_clip);
                        }
                    }
                }
                line_begin = s;
            }
            if (c == '\r')
                continue;
        }
    }

    if (line_begin < text_end)
    {
        if (raqm_complex && library != NULL && face != NULL && raqm_buf != NULL)
        {
            raqm_clear_contents(raqm_buf);
            size_t q_count;
            raqm_glyph_t *qglyphs;

            if (raqm_buf != NULL)
            {
                int mystrlength = text_end - line_begin;

                if (raqm_set_text_utf8(raqm_buf, line_begin, mystrlength) &&
                    raqm_set_freetype_face(raqm_buf, face) &&
                    raqm_set_par_direction(raqm_buf, dir) &&
                    raqm_set_language(raqm_buf, "fa", 0, mystrlength) &&
                    raqm_layout(raqm_buf))
                {
                    qglyphs = raqm_get_glyphs(raqm_buf, &q_count);
                    for (size_t i = 0; i < q_count; i++)
                    {
                        const ImFontGlyph *qglyph = g.Font->FindGlyphRaqm(qglyphs[i].index);
                        char mchar[4];
                        int charL = utf8_encode(mchar, qglyph->Codepoint);
                        finalText.append(mchar);
                    }
                }
            }
        }
    }

    const char* ctext= finalText.c_str();
    // *out_text_begin = ctext;
    *out_text_length = strlen(ctext);
    // *out_text_end = ctext + ctext(ctext);
    printf("%s (%d) (%s) <-%d, ",text_begin,strlen(text_begin), ctext,ctext);
    return finalText;
}
