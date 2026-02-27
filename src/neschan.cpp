// neschan.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "neschan.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

using namespace std;

inline uint32_t make_argb(uint8_t r, uint8_t g, uint8_t b)
{
    return (r << 16) | (g << 8) | b;
}

#define JOYSTICK_DEADZONE 8000

class neschan_exception : runtime_error
{
public :
    neschan_exception(const char *msg)
        :runtime_error(string(msg))
    {}
};

class sdl_keyboard_controller : public nes_input_device
{
public:
    sdl_keyboard_controller()
    {
    }

    virtual nes_button_flags poll_status()
    {
        return sdl_keyboard_controller::get_status();
    }

    ~sdl_keyboard_controller()
    {
    }

public:
    static nes_button_flags get_status()
    {
        const Uint8 *states = SDL_GetKeyboardState(NULL);

        uint8_t flags = 0;
        for (int i = 0; i < 8; i++)
        {
            flags <<= 1;
            flags |= states[s_buttons[i]];
        }

        return (nes_button_flags)flags;
    }

private:

    static const SDL_Scancode s_buttons[];
};

const SDL_Scancode sdl_keyboard_controller::s_buttons[] = {
    SDL_SCANCODE_L,
    SDL_SCANCODE_J,
    SDL_SCANCODE_SPACE,
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_W,
    SDL_SCANCODE_S,
    SDL_SCANCODE_A,
    SDL_SCANCODE_D
};

class sdl_game_controller : public nes_input_device
{
public :
    sdl_game_controller(int id)
        :_id(id)
    {
        _controller = SDL_GameControllerOpen(_id);
        if (_controller == nullptr)
        {
            auto error = SDL_GetError();
            NES_LOG("[NESCHAN_CONTROLLER] Failed to open Game Controller #" << id << ":" << error);
            throw neschan_exception(error);
        }

        NES_LOG("[NESCHAN] Opening GameController #" << std::dec << _id << "..." );
        auto name = SDL_GameControllerName(_controller);
        NES_LOG("    Name = " << name);
        char *mapping = SDL_GameControllerMapping(_controller);
        NES_LOG("    Mapping = " << mapping);

        // Don't need game controller events
        SDL_GameControllerEventState(SDL_DISABLE);
    }

    virtual nes_button_flags poll_status()
    {
        SDL_GameControllerUpdate();

        uint8_t flags = 0;
        for (int i = 0; i < 8; i++)
        {
            flags <<= 1;
            flags |= SDL_GameControllerGetButton(_controller, s_buttons[i]);
        }

        if (_id == 0)
        {
            // Also poll keyboard as well for main player
            flags |= sdl_keyboard_controller::get_status();
        }

        return (nes_button_flags)flags;
    }

    ~sdl_game_controller()
    {
        NES_LOG("[NESCHAN] Closing GameController #" << std::dec << _id << "..." );
        SDL_GameControllerClose(_controller);
        _controller = nullptr;
    }

private :
    int _id;
    SDL_GameController * _controller;

    static const SDL_GameControllerButton s_buttons[];
};

const SDL_GameControllerButton sdl_game_controller::s_buttons[] = {
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_GUIDE,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};

struct app_options
{
    const char *rom_path = nullptr;
    const char *replay_log_path = nullptr;
    bool headless = false;
    int max_frames = -1;
};

bool parse_args(int argc, char *argv[], app_options &options)
{
    if (argc < 2)
        return false;

    options.rom_path = argv[1];

    for (int i = 2; i < argc; ++i)
    {
        string arg = argv[i];
        if (arg == "--headless")
        {
            options.headless = true;
        }
        else if (arg == "--replay" && i + 1 < argc)
        {
            options.replay_log_path = argv[++i];
        }
        else if (arg == "--max-frames" && i + 1 < argc)
        {
            options.max_frames = atoi(argv[++i]);
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool parse_replay_log(const char *path, vector<nes_button_flags> &stream)
{
    ifstream in(path);
    if (!in.is_open())
        return false;

    map<uint32_t, nes_button_flags> frame_to_flags;

    string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream parser(line);
        string frame_token;
        string flags_token;

        if (!(parser >> frame_token >> flags_token))
            continue;

        uint32_t frame = static_cast<uint32_t>(std::stoul(frame_token, nullptr, 0));
        uint32_t flags = static_cast<uint32_t>(std::stoul(flags_token, nullptr, 0));

        frame_to_flags[frame] = static_cast<nes_button_flags>(flags & 0xff);
    }

    if (frame_to_flags.empty())
    {
        stream.clear();
        return true;
    }

    uint32_t last_frame = frame_to_flags.rbegin()->first;
    stream.assign(last_frame + 1, nes_button_flags_none);
    for (auto kv : frame_to_flags)
        stream[kv.first] = kv.second;

    return true;
}

int main(int argc, char *argv[])
{
    app_options options;
    if (!parse_args(argc, argv, options))
    {
        cout << "Usage: neschan <rom_file_path> [--replay <input_log>] [--headless] [--max-frames <n>]" << endl;
        cout << "Input log format: <frame_index> <button_flags> (e.g. '120 0x08' for W/UP)." << endl;
        return -1;
    }

    Uint32 sdl_flags = options.headless ? 0 : SDL_INIT_EVERYTHING;
    if (SDL_Init(sdl_flags) < 0)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "SDL_Init Error",
            SDL_GetError(),
            NULL);
        return -1;
    }

    SDL_Window *sdl_window = nullptr;
    SDL_Renderer *sdl_renderer = nullptr;
    SDL_Texture *sdl_texture = nullptr;

    if (!options.headless)
    {
        SDL_CreateWindowAndRenderer(PPU_SCREEN_X * 2, PPU_SCREEN_Y * 2, SDL_WINDOW_SHOWN, &sdl_window, &sdl_renderer);

        SDL_SetWindowTitle(sdl_window, "NESChan v0.1 by yizhang82");

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");  // make the scaled rendering look smoother.
        SDL_RenderSetLogicalSize(sdl_renderer, PPU_SCREEN_X, PPU_SCREEN_Y);

        sdl_texture = SDL_CreateTexture(
            sdl_renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            PPU_SCREEN_X, PPU_SCREEN_Y);
    }

    INIT_TRACE("neschan.log");

    nes_system system;

    system.power_on();

    try
    {
        system.load_rom(options.rom_path, nes_rom_exec_mode_reset);
    }
    catch (std::exception ex)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "ROM load error",
            ex.what(),
            NULL);
        return -1;
    }

    vector<uint32_t> pixels(PPU_SCREEN_Y * PPU_SCREEN_X);

    static uint32_t palette[] =
    {
        make_argb(84,  84,  84),    make_argb(0,  30, 116),    make_argb(8,  16, 144),    make_argb(48,   0, 136),   make_argb(68,   0, 100),   make_argb(92,   0,  48),   make_argb(84,   4,   0),   make_argb(60,  24,   0),   make_argb(32,  42,   0),   make_argb(8,  58,   0),   make_argb(0,  64,   0),    make_argb(0,  60,   0),    make_argb(0,  50,  60),    make_argb(0,   0,   0),   make_argb(0, 0, 0), make_argb(0, 0, 0),
        make_argb(152, 150, 152),   make_argb(8,  76, 196),    make_argb(48,  50, 236),   make_argb(92,  30, 228),   make_argb(136,  20, 176),  make_argb(160,  20, 100),  make_argb(152,  34,  32),  make_argb(120,  60,   0),  make_argb(84,  90,   0),   make_argb(40, 114,   0),  make_argb(8, 124,   0),    make_argb(0, 118,  40),    make_argb(0, 102, 120),    make_argb(0,   0,   0),   make_argb(0, 0, 0), make_argb(0, 0, 0),
        make_argb(236, 238, 236),   make_argb(76, 154, 236),   make_argb(120, 124, 236),  make_argb(176,  98, 236),  make_argb(228,  84, 236),  make_argb(236,  88, 180),  make_argb(236, 106, 100),  make_argb(212, 136,  32),  make_argb(160, 170,   0),  make_argb(116, 196,   0), make_argb(76, 208,  32),   make_argb(56, 204, 108),   make_argb(56, 180, 204),   make_argb(60,  60,  60),  make_argb(0, 0, 0), make_argb(0, 0, 0),
        make_argb(236, 238, 236),   make_argb(168, 204, 236),  make_argb(188, 188, 236),  make_argb(212, 178, 236),  make_argb(236, 174, 236),  make_argb(236, 174, 212),  make_argb(236, 180, 176),  make_argb(228, 196, 144),  make_argb(204, 210, 120),  make_argb(180, 222, 120), make_argb(168, 226, 144),  make_argb(152, 226, 180),  make_argb(160, 214, 228),  make_argb(160, 162, 160), make_argb(0, 0, 0), make_argb(0, 0, 0)
    };
    assert(sizeof(palette) == sizeof(uint32_t) * 0x40);

    vector<nes_button_flags> replay_stream;
    if (options.replay_log_path != nullptr)
    {
        if (!parse_replay_log(options.replay_log_path, replay_stream))
        {
            cerr << "Failed to open replay log: " << options.replay_log_path << endl;
            return -1;
        }

        system.input()->register_input_stream(0, replay_stream);
    }
    else
    {
        int num_joysticks = SDL_NumJoysticks();
        NES_LOG("[NESCHAN] " << num_joysticks << " JoySticks detected.");
        if (num_joysticks == 0)
        {
            system.input()->register_input(0, std::make_shared<sdl_keyboard_controller>());
        }
        else
        {
            for (int i = 0; i < num_joysticks; i++)
            {
                if (i < NES_MAX_PLAYER)
                    system.input()->register_input(i, std::make_shared<sdl_game_controller>(i));
            }
        }
    }

    SDL_Event sdl_event;
    Uint64 prev_counter = SDL_GetPerformanceCounter();
    Uint64 count_per_second = SDL_GetPerformanceFrequency();

    const nes_cycle_t cpu_cycles_per_frame = nes_cycle_t(NES_CLOCK_HZ / 60);
    int frame_index = 0;

    //
    // Game main loop
    //
    bool quit = false;
    while (!quit)
    {
        if (!options.headless)
        {
            while (SDL_PollEvent(&sdl_event) != 0)
            {
                switch (sdl_event.type)
                {
                    case SDL_QUIT:
                        quit = true;
                        break;
                }
            }
        }

        nes_cycle_t cpu_cycles = cpu_cycles_per_frame;
        if (!options.headless)
        {
            Uint64 cur_counter = SDL_GetPerformanceCounter();
            Uint64 delta_ticks = cur_counter - prev_counter;
            prev_counter = cur_counter;
            if (delta_ticks == 0)
                delta_ticks = 1;
            cpu_cycles = ms_to_nes_cycle((double)delta_ticks * 1000 / count_per_second);

            // Avoids a scenario where the loop keeps getting longer
            if (cpu_cycles > nes_cycle_t(NES_CLOCK_HZ))
                cpu_cycles = nes_cycle_t(NES_CLOCK_HZ);
        }

        for (nes_cycle_t i = nes_cycle_t(0); i < cpu_cycles; ++i)
            system.step(nes_cycle_t(1));

        if (!options.headless)
        {
            uint32_t *cur_pixel = pixels.data();
            uint8_t *frame_buffer = system.ppu()->frame_buffer();
            for (int y = 0; y < PPU_SCREEN_Y; ++y)
            {
                for (int x = 0; x < PPU_SCREEN_X; ++x)
                {
                    *cur_pixel = palette[(*frame_buffer & 0xff)];
                    frame_buffer++;
                    cur_pixel++;
                }
            }

            SDL_UpdateTexture(sdl_texture, NULL, pixels.data(), PPU_SCREEN_X * sizeof(uint32_t));
            SDL_RenderClear(sdl_renderer);
            SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
            SDL_RenderPresent(sdl_renderer);
        }

        frame_index++;
        if (options.max_frames >= 0 && frame_index >= options.max_frames)
            quit = true;
    }

    system.input()->unregister_all_inputs();

    if (!options.headless)
    {
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyTexture(sdl_texture);
        SDL_DestroyWindow(sdl_window);
    }

    SDL_Quit();

    return 0;
}
