#include "stdafx.h"
#include <cstring>

#include "nes_ppu.h"
#include "nes_system.h"
#include "nes_memory.h"

namespace
{
    template<typename T>
    void write_value(vector<uint8_t> &out, T value)
    {
        for (size_t i = 0; i < sizeof(T); ++i)
            out.push_back(uint8_t((uint64_t(value) >> (i * 8)) & 0xff));
    }

    template<typename T>
    bool read_value(const uint8_t *data, size_t size, size_t &offset, T &value)
    {
        if (offset + sizeof(T) > size)
            return false;

        uint64_t v = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            v |= uint64_t(data[offset++]) << (i * 8);

        value = T(v);
        return true;
    }
}

nes_ppu_protect::nes_ppu_protect(nes_ppu *ppu)
{
    _ppu = ppu;
    _ppu->set_protect(true);
}

nes_ppu_protect::~nes_ppu_protect()
{
    _ppu->set_protect(false);
}

nes_ppu::~nes_ppu()
{
    _oam = nullptr;
    _vram = nullptr;
}

void nes_ppu::write_OAMDMA(uint8_t val)
{
    // @TODO - CPU is suspended and take 513/514 cycle
    _system->cpu()->request_dma((uint16_t(val) << 8));
}

void nes_ppu::oam_dma(uint16_t addr)
{
    if (_oam_addr == 0)
    {
        // simple case - copy the 0x100 bytes directly
        _system->ram()->get_bytes(_oam.get(), PPU_OAM_SIZE, addr, PPU_OAM_SIZE);
    }
    else
    {
        // the copy starts at _oam_addr and wraps around
        int copy_before_wrap = 0x100 - _oam_addr;
        _system->ram()->get_bytes(_oam.get() + _oam_addr, copy_before_wrap, addr, copy_before_wrap);
        _system->ram()->get_bytes(_oam.get(), PPU_OAM_SIZE - copy_before_wrap, addr + copy_before_wrap, PPU_OAM_SIZE - copy_before_wrap);
    }
}

void nes_ppu::load_mapper(shared_ptr<nes_mapper> &mapper)
{
    // unset previous mapper
    _mapper = nullptr;

    // Give mapper a chance to copy all the bytes needed
    mapper->on_load_ppu(*this);

    nes_mapper_info info;
    mapper->get_info(info);
    set_mirroring(info.flags);

    _mapper = mapper;
}

void nes_ppu::set_mirroring(nes_mapper_flags flags)
{
    _mirroring_flags = nes_mapper_flags(flags & nes_mapper_flags_mirroring_mask);
}

void nes_ppu::init()
{
    // PPUCTRL data
    _name_tbl_addr = 0;
    _bg_pattern_tbl_addr = 0;
    _sprite_pattern_tbl_addr = 0;
    _ppu_addr_inc = 1;
    _vblank_nmi = false;
    _use_8x16_sprite = false;
    _sprite_height = 8;

    // PPUMASK
    _show_bg = false;
    _show_sprites = false;
    _gray_scale_mode = false;

    // PPUSTATUS
    _latch = 0;
    _sprite_overflow = false;
    _vblank_started = false;
    _sprite_0_hit = false;

    // OAMADDR, OAMDATA
    _oam_addr = 0;

    // PPUSCROLL
    _addr_toggle = false;

    // PPUADDR
    _ppu_addr = 0;
    _temp_ppu_addr = 0;
    _fine_x_scroll = 0;
    _scroll_y = 0;

    // PPUDATA
    _vram_read_buf = 0;

    _master_cycle = nes_cycle_t(0);
    _scanline_cycle = nes_cycle_t(0);
    _cur_scanline = 0;
    _frame_count = 0;

    _protect_register = false;
    _stop_after_frame = -1;
    _auto_stop = false;

    _mask_oam_read = false;
    _frame_buffer = _frame_buffer_1;
    memset(_frame_buffer_1, 0, sizeof(_frame_buffer_1));
    memset(_frame_buffer_2, 0, sizeof(_frame_buffer_2));
    memset(_frame_buffer_bg, 0, sizeof(_frame_buffer_bg));

    _last_sprite_id = 0;
    _has_sprite_0 = 0;
    _mask_oam_read = 0; 
}

void nes_ppu::reset()
{
    init();
}



void nes_ppu::serialize(vector<uint8_t> &out) const
{
    auto push16 = [&](uint16_t v) { out.push_back(uint8_t(v & 0xff)); out.push_back(uint8_t((v >> 8) & 0xff)); };
    auto push32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(uint8_t((v >> (i * 8)) & 0xff)); };
    auto push64 = [&](uint64_t v) { for (int i = 0; i < 8; ++i) out.push_back(uint8_t((v >> (i * 8)) & 0xff)); };

    push16(_name_tbl_addr); push16(_bg_pattern_tbl_addr); push16(_sprite_pattern_tbl_addr); push16(_ppu_addr_inc);
    out.push_back(_vblank_nmi ? 1 : 0); out.push_back(_use_8x16_sprite ? 1 : 0); out.push_back(_sprite_height);
    out.push_back(_show_bg ? 1 : 0); out.push_back(_show_sprites ? 1 : 0); out.push_back(_gray_scale_mode ? 1 : 0);
    out.push_back(_latch); out.push_back(_sprite_overflow ? 1 : 0); out.push_back(_vblank_started ? 1 : 0); out.push_back(_sprite_0_hit ? 1 : 0);
    out.push_back(_oam_addr); out.push_back(_addr_toggle ? 1 : 0);
    push16(_ppu_addr); push16(_temp_ppu_addr); out.push_back(_fine_x_scroll); out.push_back(_scroll_y); out.push_back(_vram_read_buf);
    push64(uint64_t(_master_cycle.count())); push64(uint64_t(_scanline_cycle.count()));
    push32(uint32_t(_cur_scanline)); push32(_frame_count);
    out.push_back(_protect_register ? 1 : 0); push32(_stop_after_frame); out.push_back(_auto_stop ? 1 : 0);
    out.push_back(_tile_index); out.push_back(_tile_palette_bit32); out.push_back(_bitplane0);
    out.push_back(_frame_buffer == _frame_buffer_1 ? 1 : 2);
    out.insert(out.end(), _frame_buffer_1, _frame_buffer_1 + sizeof(_frame_buffer_1));
    out.insert(out.end(), _frame_buffer_2, _frame_buffer_2 + sizeof(_frame_buffer_2));
    out.insert(out.end(), _frame_buffer_bg, _frame_buffer_bg + sizeof(_frame_buffer_bg));
    out.insert(out.end(), _pixel_cycle, _pixel_cycle + sizeof(_pixel_cycle));
    out.push_back(_shift_reg); out.push_back(_x_offset);

    out.push_back(_last_sprite_id); out.push_back(_has_sprite_0 ? 1 : 0); out.push_back(_mask_oam_read ? 1 : 0); out.push_back(_sprite_pos_y);
    out.insert(out.end(), (uint8_t*)_sprite_buf, (uint8_t*)_sprite_buf + sizeof(_sprite_buf));
    out.push_back(uint8_t(_mirroring_flags));

    out.insert(out.end(), _vram.get(), _vram.get() + PPU_VRAM_SIZE);
    out.insert(out.end(), _oam.get(), _oam.get() + PPU_OAM_SIZE);
}

bool nes_ppu::deserialize(const uint8_t *data, size_t size, size_t &offset)
{
    auto need = [&](size_t n){ return offset + n <= size; };
    auto rd16 = [&](){ uint16_t v = uint16_t(data[offset]) | (uint16_t(data[offset + 1]) << 8); offset += 2; return v; };
    auto rd32 = [&](){ uint32_t v = 0; for (int i=0;i<4;++i) v |= (uint32_t(data[offset++]) << (i*8)); return v; };
    auto rd64 = [&](){ uint64_t v = 0; for (int i=0;i<8;++i) v |= (uint64_t(data[offset++]) << (i*8)); return v; };

    size_t fixed = 79 + sizeof(_frame_buffer_1) + sizeof(_frame_buffer_2) + sizeof(_frame_buffer_bg) + sizeof(_pixel_cycle) + sizeof(_sprite_buf) + PPU_VRAM_SIZE + PPU_OAM_SIZE;
    if (!need(fixed)) return false;

    _name_tbl_addr = rd16(); _bg_pattern_tbl_addr = rd16(); _sprite_pattern_tbl_addr = rd16(); _ppu_addr_inc = rd16();
    _vblank_nmi = data[offset++] != 0; _use_8x16_sprite = data[offset++] != 0; _sprite_height = data[offset++];
    _show_bg = data[offset++] != 0; _show_sprites = data[offset++] != 0; _gray_scale_mode = data[offset++] != 0;
    _latch = data[offset++]; _sprite_overflow = data[offset++] != 0; _vblank_started = data[offset++] != 0; _sprite_0_hit = data[offset++] != 0;
    _oam_addr = data[offset++]; _addr_toggle = data[offset++] != 0;
    _ppu_addr = rd16(); _temp_ppu_addr = rd16(); _fine_x_scroll = data[offset++]; _scroll_y = data[offset++]; _vram_read_buf = data[offset++];
    _master_cycle = nes_cycle_t((int64_t)rd64()); _scanline_cycle = nes_ppu_cycle_t((int64_t)rd64());
    _cur_scanline = (int)rd32(); _frame_count = rd32();
    _protect_register = data[offset++] != 0; _stop_after_frame = rd32(); _auto_stop = data[offset++] != 0;
    _tile_index = data[offset++]; _tile_palette_bit32 = data[offset++]; _bitplane0 = data[offset++];
    uint8_t frame_sel = data[offset++];
    memcpy_s(_frame_buffer_1, sizeof(_frame_buffer_1), data + offset, sizeof(_frame_buffer_1)); offset += sizeof(_frame_buffer_1);
    memcpy_s(_frame_buffer_2, sizeof(_frame_buffer_2), data + offset, sizeof(_frame_buffer_2)); offset += sizeof(_frame_buffer_2);
    memcpy_s(_frame_buffer_bg, sizeof(_frame_buffer_bg), data + offset, sizeof(_frame_buffer_bg)); offset += sizeof(_frame_buffer_bg);
    memcpy_s(_pixel_cycle, sizeof(_pixel_cycle), data + offset, sizeof(_pixel_cycle)); offset += sizeof(_pixel_cycle);
    _shift_reg = data[offset++]; _x_offset = data[offset++];
    _last_sprite_id = data[offset++]; _has_sprite_0 = data[offset++] != 0; _mask_oam_read = data[offset++] != 0; _sprite_pos_y = data[offset++];
    memcpy_s(_sprite_buf, sizeof(_sprite_buf), data + offset, sizeof(_sprite_buf)); offset += sizeof(_sprite_buf);
    _mirroring_flags = (nes_mapper_flags)data[offset++];

    memcpy_s(_vram.get(), PPU_VRAM_SIZE, data + offset, PPU_VRAM_SIZE); offset += PPU_VRAM_SIZE;
    memcpy_s(_oam.get(), PPU_OAM_SIZE, data + offset, PPU_OAM_SIZE); offset += PPU_OAM_SIZE;

    _frame_buffer = (frame_sel == 2) ? _frame_buffer_2 : _frame_buffer_1;
    return true;
}

void nes_ppu::power_on(nes_system *system)
{
    NES_TRACE1("[NES_PPU] POWER ON");

    init();

    _system = system;

    NES_TRACE3("[NES_PPU] SCANLINE " << std::dec << _cur_scanline << " ------ ");
}

void nes_ppu::serialize(vector<uint8_t> &out) const
{
    write_value(out, uint32_t(PPU_VRAM_SIZE));
    out.insert(out.end(), _vram.get(), _vram.get() + PPU_VRAM_SIZE);
    write_value(out, uint32_t(PPU_OAM_SIZE));
    out.insert(out.end(), _oam.get(), _oam.get() + PPU_OAM_SIZE);

    write_value(out, _name_tbl_addr);
    write_value(out, _bg_pattern_tbl_addr);
    write_value(out, _sprite_pattern_tbl_addr);
    write_value(out, _ppu_addr_inc);
    write_value(out, uint8_t(_vblank_nmi));
    write_value(out, uint8_t(_use_8x16_sprite));
    write_value(out, _sprite_height);
    write_value(out, uint8_t(_show_bg));
    write_value(out, uint8_t(_show_sprites));
    write_value(out, uint8_t(_gray_scale_mode));
    write_value(out, _latch);
    write_value(out, uint8_t(_sprite_overflow));
    write_value(out, uint8_t(_vblank_started));
    write_value(out, uint8_t(_sprite_0_hit));
    write_value(out, _oam_addr);
    write_value(out, uint8_t(_addr_toggle));
    write_value(out, _ppu_addr);
    write_value(out, _temp_ppu_addr);
    write_value(out, _fine_x_scroll);
    write_value(out, _scroll_y);
    write_value(out, _vram_read_buf);
    write_value(out, _master_cycle.count());
    write_value(out, _scanline_cycle.count());
    write_value(out, int32_t(_cur_scanline));
    write_value(out, _frame_count);
    write_value(out, uint8_t(_protect_register));
    write_value(out, _stop_after_frame);
    write_value(out, int32_t(_auto_stop));
    write_value(out, _tile_index);
    write_value(out, _tile_palette_bit32);
    write_value(out, _bitplane0);
    write_value(out, _shift_reg);
    write_value(out, _x_offset);
    write_value(out, _last_sprite_id);
    write_value(out, uint8_t(_has_sprite_0));
    write_value(out, uint8_t(_mask_oam_read));
    write_value(out, _sprite_pos_y);
    write_value(out, uint16_t(_mirroring_flags));

    write_value(out, uint8_t(_frame_buffer == _frame_buffer_1 ? 1 : 2));
    out.insert(out.end(), _frame_buffer_1, _frame_buffer_1 + sizeof(_frame_buffer_1));
    out.insert(out.end(), _frame_buffer_2, _frame_buffer_2 + sizeof(_frame_buffer_2));
    out.insert(out.end(), _frame_buffer_bg, _frame_buffer_bg + sizeof(_frame_buffer_bg));
    out.insert(out.end(), _pixel_cycle, _pixel_cycle + sizeof(_pixel_cycle));
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(&_sprite_buf[0]), reinterpret_cast<const uint8_t*>(&_sprite_buf[0]) + sizeof(_sprite_buf));
}

bool nes_ppu::deserialize(const uint8_t *data, size_t size, size_t &offset)
{
    uint8_t b = 0;
    uint32_t len = 0;
    int64_t master_cycle = 0;
    int64_t scanline_cycle = 0;
    int32_t cur_scanline = 0;
    int32_t auto_stop = 0;
    uint16_t mirror_flags = 0;
    uint8_t frame_buffer_id = 0;

    if (!read_value(data, size, offset, len) || len != PPU_VRAM_SIZE || offset + len > size) return false;
    memcpy_s(_vram.get(), PPU_VRAM_SIZE, data + offset, len);
    offset += len;
    if (!read_value(data, size, offset, len) || len != PPU_OAM_SIZE || offset + len > size) return false;
    memcpy_s(_oam.get(), PPU_OAM_SIZE, data + offset, len);
    offset += len;

    if (!read_value(data, size, offset, _name_tbl_addr)) return false;
    if (!read_value(data, size, offset, _bg_pattern_tbl_addr)) return false;
    if (!read_value(data, size, offset, _sprite_pattern_tbl_addr)) return false;
    if (!read_value(data, size, offset, _ppu_addr_inc)) return false;
    if (!read_value(data, size, offset, b)) return false; _vblank_nmi = (b != 0);
    if (!read_value(data, size, offset, b)) return false; _use_8x16_sprite = (b != 0);
    if (!read_value(data, size, offset, _sprite_height)) return false;
    if (!read_value(data, size, offset, b)) return false; _show_bg = (b != 0);
    if (!read_value(data, size, offset, b)) return false; _show_sprites = (b != 0);
    if (!read_value(data, size, offset, b)) return false; _gray_scale_mode = (b != 0);
    if (!read_value(data, size, offset, _latch)) return false;
    if (!read_value(data, size, offset, b)) return false; _sprite_overflow = (b != 0);
    if (!read_value(data, size, offset, b)) return false; _vblank_started = (b != 0);
    if (!read_value(data, size, offset, b)) return false; _sprite_0_hit = (b != 0);
    if (!read_value(data, size, offset, _oam_addr)) return false;
    if (!read_value(data, size, offset, b)) return false; _addr_toggle = (b != 0);
    if (!read_value(data, size, offset, _ppu_addr)) return false;
    if (!read_value(data, size, offset, _temp_ppu_addr)) return false;
    if (!read_value(data, size, offset, _fine_x_scroll)) return false;
    if (!read_value(data, size, offset, _scroll_y)) return false;
    if (!read_value(data, size, offset, _vram_read_buf)) return false;
    if (!read_value(data, size, offset, master_cycle)) return false; _master_cycle = nes_cycle_t(master_cycle);
    if (!read_value(data, size, offset, scanline_cycle)) return false; _scanline_cycle = nes_ppu_cycle_t(scanline_cycle);
    if (!read_value(data, size, offset, cur_scanline)) return false; _cur_scanline = cur_scanline;
    if (!read_value(data, size, offset, _frame_count)) return false;
    if (!read_value(data, size, offset, b)) return false; _protect_register = (b != 0);
    if (!read_value(data, size, offset, _stop_after_frame)) return false;
    if (!read_value(data, size, offset, auto_stop)) return false; _auto_stop = auto_stop;
    if (!read_value(data, size, offset, _tile_index)) return false;
    if (!read_value(data, size, offset, _tile_palette_bit32)) return false;
    if (!read_value(data, size, offset, _bitplane0)) return false;
    if (!read_value(data, size, offset, _shift_reg)) return false;
    if (!read_value(data, size, offset, _x_offset)) return false;
    if (!read_value(data, size, offset, _last_sprite_id)) return false;
    if (!read_value(data, size, offset, b)) return false; _has_sprite_0 = (b != 0);
    if (!read_value(data, size, offset, b)) return false; _mask_oam_read = (b != 0);
    if (!read_value(data, size, offset, _sprite_pos_y)) return false;
    if (!read_value(data, size, offset, mirror_flags)) return false; _mirroring_flags = nes_mapper_flags(mirror_flags);

    if (!read_value(data, size, offset, frame_buffer_id)) return false;
    if (offset + sizeof(_frame_buffer_1) + sizeof(_frame_buffer_2) + sizeof(_frame_buffer_bg) + sizeof(_pixel_cycle) + sizeof(_sprite_buf) > size) return false;
    memcpy_s(_frame_buffer_1, sizeof(_frame_buffer_1), data + offset, sizeof(_frame_buffer_1)); offset += sizeof(_frame_buffer_1);
    memcpy_s(_frame_buffer_2, sizeof(_frame_buffer_2), data + offset, sizeof(_frame_buffer_2)); offset += sizeof(_frame_buffer_2);
    memcpy_s(_frame_buffer_bg, sizeof(_frame_buffer_bg), data + offset, sizeof(_frame_buffer_bg)); offset += sizeof(_frame_buffer_bg);
    memcpy_s(_pixel_cycle, sizeof(_pixel_cycle), data + offset, sizeof(_pixel_cycle)); offset += sizeof(_pixel_cycle);
    memcpy_s(&_sprite_buf[0], sizeof(_sprite_buf), data + offset, sizeof(_sprite_buf)); offset += sizeof(_sprite_buf);
    _frame_buffer = (frame_buffer_id == 1) ? _frame_buffer_1 : _frame_buffer_2;

    return true;
}

// fetching tile for current line
void nes_ppu::fetch_tile()
{
    auto scanline_render_cycle = nes_ppu_cycle_t(0);
    uint16_t cur_scanline = _cur_scanline;
    if (_scanline_cycle > nes_ppu_cycle_t(320))
    {
        // this is prefetch cycle 321~336 for next scanline
        scanline_render_cycle = _scanline_cycle - nes_ppu_cycle_t(321);
        cur_scanline = (cur_scanline + 1) % PPU_SCREEN_Y ;
    }
    else
    {
        // account for the prefetch happened in earlier scanline
        scanline_render_cycle = (_scanline_cycle - nes_ppu_cycle_t(1) + nes_ppu_cycle_t(16));
    }

    auto data_access_cycle = scanline_render_cycle % 8;

    // which of 8 rows witin a tile
    uint8_t tile_row_index = (cur_scanline + _scroll_y) % 8;

    if (data_access_cycle == nes_ppu_cycle_t(0))
    {
        // fetch nametable byte for current 8-pixel-tile
        // http://wiki.nesdev.com/w/index.php/PPU_nametables
        uint16_t name_tbl_addr = (_ppu_addr & 0xfff) | 0x2000;
        _tile_index = read_byte(name_tbl_addr);
    }
    else if (data_access_cycle == nes_ppu_cycle_t(2))
    {
        // fetch attribute table byte
        // each attribute pixel is 4 quadrant of 2x2 tile (so total of 8x8) tile
        // the result color byte is 2-bit (bit 3/2) for each quadrant
        // http://wiki.nesdev.com/w/index.php/PPU_attribute_tables
        // http://wiki.nesdev.com/w/index.php/PPU_scrolling#Wrapping_around
        uint8_t tile_column = _ppu_addr & 0x1f;         // YY YYYX XXXX = 1 1111
        uint8_t tile_row = (_ppu_addr & 0x3e0) >> 5;    // YY YYYX XXXX = 11 1110 0000
        uint8_t tile_attr_column = (tile_column >> 2) & 0x7;
        uint8_t tile_attr_row = (tile_row >> 2) & 0x7;
        uint16_t attr_tbl_addr = 0x23c0 | (_ppu_addr & 0x0c00) | (tile_attr_row << 3) | tile_attr_column;
        uint8_t color_byte = read_byte(attr_tbl_addr);

        // each quadrant has 2x2 tile and each row/column has 4 tiles, so divide by 2 (& 0x2 is faster)
        uint8_t _quadrant_id = (tile_row & 0x2) + ((tile_column & 0x2) >> 1);
        uint8_t color_bit32 = (color_byte & (0x3 << (_quadrant_id * 2))) >> (_quadrant_id * 2); 
        _tile_palette_bit32 = color_bit32 << 2;
    }
    else if (data_access_cycle == nes_ppu_cycle_t(4))
    {
        // Pattern table is area of memory define all the tiles make up background and sprites.
        // Think it as "lego blocks" that you can build up your background and sprites which 
        // simply consists of indexes. It is quite convoluted by today's standards but it is 
        // just a space saving technique.
        // http://wiki.nesdev.com/w/index.php/PPU_pattern_tables
        _bitplane0 = read_pattern_table_column(/* sprite = */false, _tile_index, /* bitplane = */ 0, tile_row_index);
    }
    else if (data_access_cycle == nes_ppu_cycle_t(6))
    {
        // fetch tilebitmap high
        // add one more cycle for memory access to skip directly to next access
        uint8_t bitplane1 = read_pattern_table_column(/* sprite = */false, _tile_index, /* bitplane = */ 1, tile_row_index);

        // for each column - bitplane0/bitplane1 has entire 8 column
        // high bit -> low bit
        int start_bit = 7;
        int end_bit = 0;
        
        int tile = (int)(scanline_render_cycle.count() - /* current_access_cycle */ 6) / 8;
        if (_fine_x_scroll > 0)
        {
            if (tile == 0)
            {
                start_bit = 7 - _fine_x_scroll;
            }
            else if (tile == 32)
            {
                // last tile
                end_bit = 7 - _fine_x_scroll + 1;
            }
            else if (tile > 32)
            {
                // no need to render more than 33 tiles
                // otherwise you'll see wrapped tiles in the begining of next line
                return;
            }
        }
        else
        {
            // We render exactly 32 tiles
            if (tile > 31) return;
        }

        for (int i = start_bit; i >= end_bit; --i)
        {
            uint8_t column_mask = 1 << i;
            uint8_t tile_palette_bit01 = ((_bitplane0 & column_mask) >> i) | ((bitplane1 & column_mask) >> i << 1);
            uint8_t color_4_bit = _tile_palette_bit32 | tile_palette_bit01;

            _pixel_cycle[i] = get_palette_color(/* is_background = */ true, color_4_bit);

            uint16_t frame_addr = uint16_t(cur_scanline) * PPU_SCREEN_X + _x_offset++;
            if (frame_addr >= sizeof(_frame_buffer_1))
                continue;
            _frame_buffer[frame_addr] = _pixel_cycle[i];

            // record the palette index just for sprite 0 hit detection
            // the detection use palette 0 instead of actual color
            _frame_buffer_bg[frame_addr] = tile_palette_bit01;
        }

        // Increment X position
        if ((_ppu_addr & 0x1f) == 0x1f)
        {
            // Wrap to the next name table
            _ppu_addr &= ~0x1f;
            _ppu_addr ^= 0x0400;
        }
        else
        {
            _ppu_addr++;
        }
    }
}

void nes_ppu::fetch_tile_pipeline()
{
    // No need to fetch anything if rendering is off
    if (!_show_bg) return;

    if (_scanline_cycle == nes_ppu_cycle_t(0))
    {
    }
    else if (_scanline_cycle < nes_ppu_cycle_t(257))
    {        
        fetch_tile();

        if (_scanline_cycle == nes_ppu_cycle_t(256))
        {
            if ((_ppu_addr & 0x7000) != 0x7000)
            {
                // Increase fine Y position (within tile)
                _ppu_addr += 0x1000;
            }
            else
            {
                _ppu_addr &= ~0x7000;

                // == row 29?
                if ((_ppu_addr & 0x3e0) != 0x3a0)
                {
                     // Increase coarse Y position (next tile)
                    _ppu_addr += 0x20;
                }
                else
                {
                    // wrap around
                    _ppu_addr &= ~0x3e0;

                    // switch to another vertical name table
                    _ppu_addr ^= 0x0800;
                }
            }
        }
    }
    else if (_scanline_cycle < nes_ppu_cycle_t(321))
    {
        if (_scanline_cycle == nes_ppu_cycle_t(257))
        {
            // Reset horizontal position
            // This includes resetting horizontal name table (2000~2400, 2800~2c00)
            // NNYY YYYX XXXX
            //  ^      ^ ^^^^
            _ppu_addr = (_ppu_addr & 0xfbe0) | (_temp_ppu_addr & ~0xfbe0);
            _x_offset = 0;
        }

        // fetch tile data for sprites on the next scanline
    }
    else if (_scanline_cycle < nes_ppu_cycle_t(337))
    {
        // first two tiles for next scanline
        fetch_tile();
    }
    else // 337->340
    {
        // fetch two bytes - no need to emulate for now
    }
}

void nes_ppu::fetch_sprite_pipeline()
{
    if (!_show_sprites)
        return;

    // sprite never show up in scanline 0
    if (_cur_scanline == 0)
        return;

    // @TODO - Sprite 0 hit testing
    if (_scanline_cycle == nes_ppu_cycle_t(0))
    {
        _last_sprite_id = 0;
        _has_sprite_0 = false;
        memset(_sprite_buf, 0xff, sizeof(_sprite_buf));
        _sprite_overflow = false;
    }
    else if (_scanline_cycle < nes_ppu_cycle_t(65))
    {
        // initialize secondary OAM (_sprite_buf)
        // we've already done this earlier and the side effect isn't observable anyway

        // NOTE: We can set this conditionaly but it's simply faster always to set it
        _mask_oam_read = true;
    }
    else if (_scanline_cycle < nes_ppu_cycle_t(257))
    {
        // fetch tile data for sprites on the next scanline

        // NOTE: We can set this conditionaly but it's simply faster always to set it
        _mask_oam_read = false;

        uint8_t sprite_cycle = (uint8_t) (_scanline_cycle.count() - 65);
        uint8_t sprite_id = sprite_cycle / 2;

        // Cycle 65-256 has more than enough cycles to read all 64 sprites - but it appears that this
        // pipeline would synchronize with the background rendering pipeline so that sprites rendering
        // happens later and can freely overwrite background if needed.
        // Skip if we are already past 64
        if (sprite_id >= PPU_SPRITE_MAX)
            return;

        if ((_scanline_cycle.count() % 2) == 0)
        {
            // even cycle - write to secondary OAM
            // if in range
            if (_sprite_pos_y + 1 <= _cur_scanline && _cur_scanline < _sprite_pos_y + 1 + _sprite_height)
            {
                if (sprite_id == 0)
                    _has_sprite_0 = true;

                if (_last_sprite_id >= PPU_ACTIVE_SPRITE_MAX)
                    _sprite_overflow = true;
                else
                    _sprite_buf[_last_sprite_id++] = *get_sprite(sprite_id);
            }
        }
        else
        {
            // odd cycle - read from primary OAM
            _sprite_pos_y = get_sprite(sprite_id)->pos_y;            
        }
    }
    else if (_scanline_cycle < nes_ppu_cycle_t(321))
    {
        uint8_t sprite_cycle = (uint8_t) (_scanline_cycle.count() - 257);
        uint8_t sprite_id = sprite_cycle / 8;
        if (sprite_cycle % 8 == 4)
        {
            if (sprite_id < _last_sprite_id)
                fetch_sprite(sprite_id);
        }
    }
    else // 337->340
    {
        // fetch two bytes - no need to emulate for now
    }
}

void nes_ppu::fetch_sprite(uint8_t sprite_id)
{
    assert(sprite_id < PPU_ACTIVE_SPRITE_MAX);

    sprite_info *sprite = &_sprite_buf[sprite_id];
    uint8_t tile_index = sprite->tile_index;

    uint8_t tile_row_index = (_cur_scanline - sprite->pos_y - 1) % _sprite_height;

    if (sprite->attr & PPU_SPRITE_ATTR_VERTICAL_FLIP)
        tile_row_index = _sprite_height - 1 - tile_row_index;

    uint8_t bitplane0, bitplane1;
    if (_use_8x16_sprite)
    {
        bitplane0 = read_pattern_table_column_8x16_sprite(tile_index, 0, tile_row_index);
        bitplane1 = read_pattern_table_column_8x16_sprite(tile_index, 1, tile_row_index);
    }
    else
    {
        bitplane0 = read_pattern_table_column(/* sprite = */ true, tile_index, 0, tile_row_index);
        bitplane1 = read_pattern_table_column(/* sprite = */ true, tile_index, 1, tile_row_index);
    }

    // bit3/2 is shared for the entire sprite (just like background attribute table)
    uint8_t palette_index_bit32 = (sprite->attr & PPU_SPRITE_ATTR_BIT32_MASK) << 2;

    // loop all bits - high -> low
    for (int i = 7; i >= 0; --i)
    {
        uint8_t column_mask = 1 << i;
        uint8_t palette_index_bit01 = ((bitplane1 & column_mask) >> i << 1) | ((bitplane0 & column_mask) >> i);

        // palette 0 is always background
        if (palette_index_bit01 == 0)
            continue;

        uint8_t palette_index = palette_index_bit32 | palette_index_bit01;

        uint8_t color = get_palette_color(/* is_background = */false, palette_index);
        uint16_t frame_addr = _cur_scanline * PPU_SCREEN_X + sprite->pos_x;
        if (sprite->attr & PPU_SPRITE_ATTR_HORIZONTAL_FLIP)
            frame_addr += i;     // low -> high in horizontal flip
        else
            frame_addr += 7 - i; // high -> low as usual

        if (frame_addr >= sizeof(_frame_buffer_1))
        {
            // part of the sprite might be over
            continue;
        }

        bool is_sprite_0 = (_has_sprite_0 && sprite_id == 0);
        bool behind_bg = sprite->attr & PPU_SPRITE_ATTR_BEHIND_BG;
        if (behind_bg || is_sprite_0)
        {
            // use the recorded 2-bit palette index for sprite 0 hit detection
            // don't use the actual color as some times game use all 0f 'black' palette to black out screen
            bool overlap = (_frame_buffer_bg[frame_addr] != 0);
            if (overlap)
            {
                if (is_sprite_0)
                {
                    // sprite 0 hit detection
                    _sprite_0_hit = true;
                }

                if (behind_bg)
                {
                    // behind background
                    continue;
                }
             }
        }

        _frame_buffer[frame_addr] = color;
    }
}

void nes_ppu::step_to(nes_cycle_t count)
{
    while (_master_cycle < count && !_system->stop_requested())
    {     
        step_ppu(nes_ppu_cycle_t(1));

        if (_cur_scanline <= 239)
        {
            fetch_tile_pipeline();
            fetch_sprite_pipeline();
        }
        else if (_cur_scanline == 240)
        {
        }
        else if (_cur_scanline < 261)
        {
            if (_cur_scanline == 241 && _scanline_cycle == nes_ppu_cycle_t(1))
            {
                NES_TRACE4("[NES_PPU] SCANLINE = 241, VBlank BEGIN");
                _vblank_started = true;
                if (_vblank_nmi)
                {
                    // Request NMI so that games can do their rendering
                    _system->cpu()->request_nmi();
                }
            }

            // @HACK - account for a race where you have LDA $2002_PPUSTATUS and end of VBLANK at the same time
            // This moves end of NMI a bit earlier to compensate for that
            if (_cur_scanline == 260 && _scanline_cycle > nes_ppu_cycle_t(341 - 12))
                _vblank_started = false;
        }
        else
        {
            if (_cur_scanline == 261)
            {
                if (_scanline_cycle == nes_ppu_cycle_t(0))
                {
                    NES_TRACE4("[NES_PPU] SCANLINE = 261, VBlank END");
                    _vblank_started = false;

                    // Reset _ppu_addr to top-left of the screen
                    // But only do so when rendering is on (otherwise it will interfer with PPUDATA writes)
                    if (_show_bg || _show_sprites)
                    {
                        _ppu_addr = _temp_ppu_addr;
                    }
                }
                else if (_scanline_cycle == nes_ppu_cycle_t(1))
                {
                    _sprite_0_hit = false;
                }
            }

            // pre-render scanline
            if (_scanline_cycle == nes_ppu_cycle_t(340) && _frame_count % 2 == 1)
            {
                // odd frame skip the last cycle
                step_ppu(nes_ppu_cycle_t(1));
            }
        }
    }
}

void nes_ppu::step_ppu(nes_ppu_cycle_t count)
{
    assert(count < PPU_SCANLINE_CYCLE);

    _master_cycle += nes_ppu_cycle_t(count);
    _scanline_cycle += nes_ppu_cycle_t(count);

    if (_scanline_cycle >= PPU_SCANLINE_CYCLE)
    {
        _scanline_cycle %= PPU_SCANLINE_CYCLE;
        _cur_scanline++;
        if (_cur_scanline >= PPU_SCANLINE_COUNT)
        {
            _cur_scanline %= PPU_SCANLINE_COUNT;
            // Frame boundary: publish the finished frame by swapping read/write buffers.
            swap_buffer();
            _frame_count++;
            NES_TRACE4("[NES_PPU] FRAME " << std::dec << _frame_count << " ------ ");

            if (_auto_stop && _frame_count > _stop_after_frame)
            {
                NES_TRACE1("[NES_PPU] FRAME exceeding " << std::dec << _stop_after_frame << " -> stopping...");
                _system->stop();
            }
        }
        NES_TRACE4("[NES_PPU] SCANLINE " << std::dec << (uint32_t) _cur_scanline << " ------ ");
    }
}
namespace
{
    template <typename T>
    void append_state(std::vector<uint8_t> &out, const T &value)
    {
        auto begin = reinterpret_cast<const uint8_t *>(&value);
        out.insert(out.end(), begin, begin + sizeof(T));
    }

    template <typename T>
    bool read_state(const std::vector<uint8_t> &in, size_t &offset, T *value)
    {
        if (offset + sizeof(T) > in.size())
            return false;

        memcpy(value, in.data() + offset, sizeof(T));
        offset += sizeof(T);
        return true;
    }
}

void nes_ppu::serialize(vector<uint8_t> &out) const
{
    out.insert(out.end(), _vram.get(), _vram.get() + PPU_VRAM_SIZE);
    out.insert(out.end(), _oam.get(), _oam.get() + PPU_OAM_SIZE);

    append_state(out, _name_tbl_addr);
    append_state(out, _bg_pattern_tbl_addr);
    append_state(out, _sprite_pattern_tbl_addr);
    append_state(out, _ppu_addr_inc);
    append_state(out, _vblank_nmi);
    append_state(out, _use_8x16_sprite);
    append_state(out, _sprite_height);
    append_state(out, _show_bg);
    append_state(out, _show_sprites);
    append_state(out, _gray_scale_mode);
    append_state(out, _latch);
    append_state(out, _sprite_overflow);
    append_state(out, _vblank_started);
    append_state(out, _sprite_0_hit);
    append_state(out, _oam_addr);
    append_state(out, _addr_toggle);
    append_state(out, _ppu_addr);
    append_state(out, _temp_ppu_addr);
    append_state(out, _fine_x_scroll);
    append_state(out, _scroll_y);
    append_state(out, _vram_read_buf);

    int64_t master_cycle = _master_cycle.count();
    int64_t scanline_cycle = _scanline_cycle.count();
    append_state(out, master_cycle);
    append_state(out, scanline_cycle);
    append_state(out, _cur_scanline);
    append_state(out, _frame_count);

    append_state(out, _protect_register);
    append_state(out, _stop_after_frame);
    append_state(out, _auto_stop);

    append_state(out, _tile_index);
    append_state(out, _tile_palette_bit32);
    append_state(out, _bitplane0);

    uint8_t frame_buffer_select = (_frame_buffer == _frame_buffer_1) ? 1 : 2;
    append_state(out, frame_buffer_select);

    out.insert(out.end(), _frame_buffer_1, _frame_buffer_1 + sizeof(_frame_buffer_1));
    out.insert(out.end(), _frame_buffer_bg, _frame_buffer_bg + sizeof(_frame_buffer_bg));
    out.insert(out.end(), _frame_buffer_2, _frame_buffer_2 + sizeof(_frame_buffer_2));
    out.insert(out.end(), _pixel_cycle, _pixel_cycle + sizeof(_pixel_cycle));

    append_state(out, _shift_reg);
    append_state(out, _x_offset);

    out.insert(out.end(), reinterpret_cast<const uint8_t *>(_sprite_buf), reinterpret_cast<const uint8_t *>(_sprite_buf) + sizeof(_sprite_buf));

    append_state(out, _last_sprite_id);
    append_state(out, _has_sprite_0);
    append_state(out, _mask_oam_read);
    append_state(out, _sprite_pos_y);
    append_state(out, _mirroring_flags);
}

bool nes_ppu::deserialize(const vector<uint8_t> &in, size_t &offset)
{
    if (offset + PPU_VRAM_SIZE + PPU_OAM_SIZE > in.size())
        return false;

    memcpy(_vram.get(), in.data() + offset, PPU_VRAM_SIZE);
    offset += PPU_VRAM_SIZE;
    memcpy(_oam.get(), in.data() + offset, PPU_OAM_SIZE);
    offset += PPU_OAM_SIZE;

    int64_t master_cycle;
    int64_t scanline_cycle;
    uint8_t frame_buffer_select;

    bool ok =
        read_state(in, offset, &_name_tbl_addr) &&
        read_state(in, offset, &_bg_pattern_tbl_addr) &&
        read_state(in, offset, &_sprite_pattern_tbl_addr) &&
        read_state(in, offset, &_ppu_addr_inc) &&
        read_state(in, offset, &_vblank_nmi) &&
        read_state(in, offset, &_use_8x16_sprite) &&
        read_state(in, offset, &_sprite_height) &&
        read_state(in, offset, &_show_bg) &&
        read_state(in, offset, &_show_sprites) &&
        read_state(in, offset, &_gray_scale_mode) &&
        read_state(in, offset, &_latch) &&
        read_state(in, offset, &_sprite_overflow) &&
        read_state(in, offset, &_vblank_started) &&
        read_state(in, offset, &_sprite_0_hit) &&
        read_state(in, offset, &_oam_addr) &&
        read_state(in, offset, &_addr_toggle) &&
        read_state(in, offset, &_ppu_addr) &&
        read_state(in, offset, &_temp_ppu_addr) &&
        read_state(in, offset, &_fine_x_scroll) &&
        read_state(in, offset, &_scroll_y) &&
        read_state(in, offset, &_vram_read_buf) &&
        read_state(in, offset, &master_cycle) &&
        read_state(in, offset, &scanline_cycle) &&
        read_state(in, offset, &_cur_scanline) &&
        read_state(in, offset, &_frame_count) &&
        read_state(in, offset, &_protect_register) &&
        read_state(in, offset, &_stop_after_frame) &&
        read_state(in, offset, &_auto_stop) &&
        read_state(in, offset, &_tile_index) &&
        read_state(in, offset, &_tile_palette_bit32) &&
        read_state(in, offset, &_bitplane0) &&
        read_state(in, offset, &frame_buffer_select);

    if (!ok)
        return false;

    if (offset + sizeof(_frame_buffer_1) + sizeof(_frame_buffer_bg) + sizeof(_frame_buffer_2) + sizeof(_pixel_cycle) + sizeof(_sprite_buf) > in.size())
        return false;

    memcpy(_frame_buffer_1, in.data() + offset, sizeof(_frame_buffer_1));
    offset += sizeof(_frame_buffer_1);
    memcpy(_frame_buffer_bg, in.data() + offset, sizeof(_frame_buffer_bg));
    offset += sizeof(_frame_buffer_bg);
    memcpy(_frame_buffer_2, in.data() + offset, sizeof(_frame_buffer_2));
    offset += sizeof(_frame_buffer_2);
    memcpy(_pixel_cycle, in.data() + offset, sizeof(_pixel_cycle));
    offset += sizeof(_pixel_cycle);

    memcpy(_sprite_buf, in.data() + offset, sizeof(_sprite_buf));
    offset += sizeof(_sprite_buf);

    ok =
        read_state(in, offset, &_shift_reg) &&
        read_state(in, offset, &_x_offset) &&
        read_state(in, offset, &_last_sprite_id) &&
        read_state(in, offset, &_has_sprite_0) &&
        read_state(in, offset, &_mask_oam_read) &&
        read_state(in, offset, &_sprite_pos_y) &&
        read_state(in, offset, &_mirroring_flags);

    if (!ok)
        return false;

    _master_cycle = nes_cycle_t(master_cycle);
    _scanline_cycle = nes_ppu_cycle_t(scanline_cycle);
    _frame_buffer = (frame_buffer_select == 1) ? _frame_buffer_1 : _frame_buffer_2;
    return true;
}
