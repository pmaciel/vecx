
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL2_rotozoom.h>

#include "osint.h"
#include "e8910.h"
#include "vecx.h"

#define EMU_TIMER 20 /* the emulators heart beats at 20 milliseconds */

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;
static SDL_Surface *screen = NULL;
static SDL_Surface *overlay_orginal_surface = NULL;
static SDL_Surface *overlay_scaled_surface = NULL;

static long scl_factor;
static long offx;
static long offy;

void osint_render(void)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if(overlay_scaled_surface){
        SDL_Rect dest_rect = {offx, offy, 0, 0};
        SDL_BlitSurface(overlay_scaled_surface, NULL, screen, &dest_rect);
    }
    SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    int v;
    for (v = 0; v < vector_draw_cnt; v++)
    {
        Uint8 c = vectors_draw[v].color * 256 / VECTREX_COLORS;
        aalineRGBA(renderer,
                   offx + vectors_draw[v].x0 / scl_factor,
                   offy + vectors_draw[v].y0 / scl_factor,
                   offx + vectors_draw[v].x1 / scl_factor,
                   offy + vectors_draw[v].y1 / scl_factor,
                   c, c, c, 0xff);
    }
    SDL_RenderPresent(renderer);
    //SDL_Flip(screen);
}

static char *cartfilename = NULL;

static void init()
{
    FILE *f;
    char *romfilename = getenv("VECTREX_ROM");
    if (romfilename == NULL)
    {
        romfilename = "rom.dat";
    }
    if (!(f = fopen(romfilename, "rb")))
    {
        perror(romfilename);
        exit(EXIT_FAILURE);
    }
    if (fread(rom, 1, sizeof(rom), f) != sizeof(rom))
    {
        printf("Invalid rom length\n");
        exit(EXIT_FAILURE);
    }
    fclose(f);

    memset(cart, 0, sizeof(cart));
    if (cartfilename)
    {
        FILE *f;
        if (!(f = fopen(cartfilename, "rb")))
        {
            perror(cartfilename);
            exit(EXIT_FAILURE);
        }
        fread(cart, 1, sizeof(cart), f);
        fclose(f);
    }
}

void screen_init(int width, int height)
{
    long sclx, scly;

    long screenx = width;
    long screeny = height;
    window = SDL_CreateWindow("VECX",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              screenx, screeny,
                              SDL_SWSURFACE);
    renderer = SDL_CreateRenderer(window, -1, 0);
    screen = SDL_CreateRGBSurface(0, screenx, screeny, 32,
                                  0x00FF0000,
                                  0x0000FF00,
                                  0x000000FF,
                                  0xFF000000);
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                screenx, screeny);

    sclx = ALG_MAX_X / screen->w;
    scly = ALG_MAX_Y / screen->h;

    scl_factor = sclx > scly ? sclx : scly;

    offx = (screenx - ALG_MAX_X / scl_factor) / 2;
    offy = (screeny - ALG_MAX_Y / scl_factor) / 2;

    if(overlay_orginal_surface){
        if(overlay_scaled_surface)
            SDL_FreeSurface(overlay_scaled_surface);
        double overlay_scale = ((double)ALG_MAX_X / (double)scl_factor) / (double)overlay_orginal_surface->w;
        overlay_scaled_surface = zoomSurface(overlay_orginal_surface, overlay_scale, overlay_scale, 0);
    }
}

static void readevents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        switch (e.type)
        {
        case SDL_QUIT:
            exit(EXIT_SUCCESS);
            break;
        case SDL_KEYDOWN:
            switch (e.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                exit(EXIT_SUCCESS);
            case SDLK_a:
                snd_regs[14] &= ~0x01;
                break;
            case SDLK_s:
                snd_regs[14] &= ~0x02;
                break;
            case SDLK_d:
                snd_regs[14] &= ~0x04;
                break;
            case SDLK_f:
                snd_regs[14] &= ~0x08;
                break;
            case SDLK_LEFT:
                alg_jch0 = 0x00;
                break;
            case SDLK_RIGHT:
                alg_jch0 = 0xff;
                break;
            case SDLK_UP:
                alg_jch1 = 0xff;
                break;
            case SDLK_DOWN:
                alg_jch1 = 0x00;
                break;
            default:
                break;
            }
            break;
        case SDL_KEYUP:
            switch (e.key.keysym.sym)
            {
            case SDLK_a:
                snd_regs[14] |= 0x01;
                break;
            case SDLK_s:
                snd_regs[14] |= 0x02;
                break;
            case SDLK_d:
                snd_regs[14] |= 0x04;
                break;
            case SDLK_f:
                snd_regs[14] |= 0x08;
                break;
            case SDLK_LEFT:
                alg_jch0 = 0x80;
                break;
            case SDLK_RIGHT:
                alg_jch0 = 0x80;
                break;
            case SDLK_UP:
                alg_jch1 = 0x80;
                break;
            case SDLK_DOWN:
                alg_jch1 = 0x80;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
    }
}

void osint_emuloop()
{
    Uint32 next_time = SDL_GetTicks() + EMU_TIMER;
    vecx_reset();
    for (;;)
    {
        vecx_emu((VECTREX_MHZ / 1000) * EMU_TIMER);
        readevents();

        {
            Uint32 now = SDL_GetTicks();
            if (now < next_time)
                SDL_Delay(next_time - now);
            else
                next_time = now;
            next_time += EMU_TIMER;
        }
    }
}

void load_overlay(const char *filename)
{
    SDL_Surface *image;
    image = IMG_Load(filename);
    if (image)
    {
        overlay_orginal_surface = image;
        fprintf(stdout, "Loaded overlay image from file=%s\n", filename);
    }
    else
    {
        fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
    }
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
        exit(-1);
    }


    if (argc > 1)
        cartfilename = argv[1];
    if (argc > 2)
        load_overlay(argv[2]);

    screen_init(330 * 3 / 2, 410 * 3 / 2);
    init();
    e8910_init_sound();
    osint_emuloop();
    e8910_done_sound();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
