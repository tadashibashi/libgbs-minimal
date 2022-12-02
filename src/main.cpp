/* =============================================================================
 * main
 * 
 * 
 * ===========================================================================*/
#include <SDL.h>
#ifdef _WIN32
    #include <SDL_main.h>
#endif



#include <iostream>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>

extern "C" {
#include <libgbs/libgbs.h>
}

SDL_AudioDeviceID adevice = 0;
SDL_AudioSpec obtained;
SDL_AudioSpec obtainedCopy;
gbs_output_buffer buffer;
std::atomic<bool> isInBackground(false), paused(false), isRunning(false);

std::mutex mutex;


// Sound callback from libgbs to SDL2
void sound_callback(struct gbs* const gbs, struct gbs_output_buffer *buf, void *priv)
{
    int overqueued = SDL_GetQueuedAudioSize(adevice) - obtained.size;

    if (overqueued > obtained.freq) {
        float delaynanos = (float)overqueued / 4.0 / obtained.freq * 1000000000.0;
        std::this_thread::sleep_for(std::chrono::nanoseconds((long)delaynanos/4));

    }
    if (SDL_QueueAudio(adevice, buf->data, buf->bytes) != 0) {
        std::cerr << "Failed to queue audio to SDL: " << SDL_GetError();
    }
}

const gbs_channel_status *chan{};

// Step callback from libgbs for client to analyze
void step_callback(gbs *gbs, cycles_t cycles, const gbs_channel_status *chan, void *priv)
{
    ::chan = chan;
}



void gbs_main(gbs *gbs)
{
    unsigned long long ticks = 0, lastTicks = 0;
    while(isRunning)
    {
        // Update
        lastTicks = ticks;
        ticks = SDL_GetTicks64();
        auto delta = ticks - lastTicks;

        int overqueued = SDL_GetQueuedAudioSize(adevice) - obtainedCopy.size;
        std::cout << "Overqueued: " << overqueued << '\n';
        if (overqueued <= 0) delta += 8; // leave a little space of overqueue in case os causes slowdown
        if (overqueued > obtainedCopy.size * 64)
            delta -= 8;
        if (delta <= 0)
            delta = 1;

        if (!paused)
        {
            mutex.lock();
            gbs_step(gbs, 33);
            mutex.unlock();
            //std::this_thread::sleep_for(std::chrono::milliseconds(17));
        }
    }

}

void pause(bool p)
{
    paused = p;
    SDL_PauseAudio(p);
    if (p)
    {
        SDL_LockAudio();
        SDL_UnlockAudio();
    }
}

int main(int argc, char *argv[])
{
    // Setup hardware
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0)
    {
        std::cerr << "SDL2 failed to init: " << SDL_GetError() << '\n';
        return 1;
    }

    // Open window
    auto window = SDL_CreateWindow("LibGBS Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, 0);
    if (!window)
    {
        std::cerr << "Failed to open window: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    // Open renderer
    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        std::cerr << "Failed to open renderer: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Open audio device
    SDL_AudioSpec aspec{};
    SDL_zero(aspec);
    aspec.samples = 512;
    aspec.channels = 2;
    aspec.format = AUDIO_S16SYS;
    aspec.freq = 48000;
    aspec.callback = nullptr;

    adevice = SDL_OpenAudioDevice(nullptr, false, &aspec, &obtained, 0);
    if (!adevice)
    {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    obtainedCopy = obtained;

    // Setup audio buffer for gbs
    buffer.bytes = obtained.samples * sizeof(int16_t) * 2;
    if (!buffer.data)
        buffer.data = (int16_t *)malloc(buffer.bytes);


    // Load gbs
    auto g = gbs_open("res/DMG-KYJ.gbs");
    if (!g)
    {
        std::cerr << "Failed to open gbs file\n";
        SDL_CloseAudioDevice(adevice);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Configure gbs
    gbs_configure_output(g, &buffer, 48000);
    gbs_configure(g, 0, 360, 3, 0, 5);
    gbs_set_sound_callback(g, sound_callback, nullptr);
    gbs_set_step_callback(g, step_callback, nullptr);
    gbs_set_filter(g, gbs_filter_type::FILTER_DMG);
    // Play the track
    gbs_init(g, 0);
    SDL_PauseAudioDevice(adevice, false);

    std::thread gbs_thd(gbs_main, g);

    // App loop
    isRunning = true;
    int track = 0;

    while(isRunning)
    {
        // Process input
        SDL_Event ev;
        while(SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
                isRunning = false;
            if (ev.type == SDL_KEYDOWN)
            {
                if (ev.key.keysym.sym == SDLK_p)
                {
                    paused = !paused;
                    pause(paused);
                }
                if (ev.key.keysym.sym == SDLK_RIGHT)
                {
                    pause(true);
                    mutex.lock();
                    gbs_init(g, ++track);
                    mutex.unlock();
                    pause(false);

                }
                if (ev.key.keysym.sym == SDLK_LEFT)
                {
                    pause(true);
                    mutex.lock();
                    gbs_init(g, --track);
                    mutex.unlock();
                    pause(false);
                }

            }
            if (ev.type == SDL_WINDOWEVENT)
            {
                if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    isInBackground = true;
                if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                    isInBackground = false;
            }
        }



        // Draw
        if (chan)
            SDL_SetRenderDrawColor(renderer, chan[0].vol * 3, chan[1].vol * 3, chan[2].vol * 3, 255);
        SDL_RenderClear(renderer);

        SDL_RenderPresent(renderer);
    }

    gbs_thd.join();

    // Cleanup
    gbs_close(g);
    free(buffer.data);
    SDL_CloseAudioDevice(adevice);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
