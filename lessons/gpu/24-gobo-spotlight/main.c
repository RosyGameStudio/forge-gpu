#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

typedef struct {
    SDL_Window *window;
    SDL_GPUDevice *device;
} app_state;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    app_state *state = (app_state *)SDL_calloc(1, sizeof(app_state));
    *appstate = state;

    state->window = SDL_CreateWindow("Lesson 24: Gobo Spotlight", 1280, 720, 0);
    if (!state->window) return SDL_APP_FAILURE;

    state->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (!state->device) return SDL_APP_FAILURE;

    if (!SDL_ClaimWindowForGPUDevice(state->device, state->window))
        return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
        return SDL_APP_SUCCESS;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) return SDL_APP_FAILURE;

    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window, &swapchain, NULL, NULL))
        return SDL_APP_FAILURE;

    if (!swapchain) {
        SDL_SubmitGPUCommandBuffer(cmd);
        return SDL_APP_CONTINUE;
    }

    SDL_GPUColorTargetInfo color;
    SDL_zero(color);
    color.texture = swapchain;
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;
    color.clear_color = (SDL_FColor){0.008f, 0.008f, 0.026f, 1.0f};

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, NULL);
    if (pass) SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(cmd))
        return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    app_state *state = (app_state *)appstate;
    if (!state) return;

    if (state->window)
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    if (state->device)
        SDL_DestroyGPUDevice(state->device);
    SDL_DestroyWindow(state->window);

    SDL_free(state);
}
