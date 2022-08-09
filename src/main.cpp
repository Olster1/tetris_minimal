#include "wl_memory.h"
#include "file_helper.cpp"
#include "lex_utf8.h"
#include "color.cpp"
#include "selectable.cpp"
#include "wl_buffer.cpp"
#include "wl_ast.cpp"
#include "font.cpp"
#include "ui.cpp"
#include "save_settings.cpp"
#include "perlin.c"
#define EASY_ANIMATION_2D_IMPLEMENTATION 1
#include "animation.c"


#include <time.h>
#include <stdlib.h>


inline char *easy_createString_printf(Memory_Arena *arena, char *formatString, ...) {

    va_list args;
    va_start(args, formatString);

    char bogus[1];
    int stringLengthToAlloc = vsnprintf(bogus, 1, formatString, args) + 1; //for null terminator, just to be sure
    
    char *strArray = pushArray(arena, stringLengthToAlloc, char);

    vsnprintf(strArray, stringLengthToAlloc, formatString, args); 

    va_end(args);

    return strArray;
}

/*
Next:


*/

struct Player {
	float3 pos;
	float2 velocity;

	float3 cameraPos;

};

#define MAX_WINDOW_COUNT 8
#define MAX_BUFFER_COUNT 256 //TODO: Allow user to open unlimited buffers


typedef enum {
	MODE_EDIT_BUFFER,
} EditorMode;

struct CollisionRect {
	Rect2f rect;

	CollisionRect(Rect2f rect) {

	}
};

typedef struct {
	bool initialized;

	Renderer renderer;

	EditorMode mode;

	Font font;

	float fontScale;

	bool draw_debug_memory_stats;

	Player player;

	Texture playerTexture;

	Texture pipeTexture;

	Texture backgroundTexture;

	int points;

	CollisionRect rects[128];

	EasyAnimation_Controller playerAnimationController;
	Animation playerIdleAnimation;

	EasyAnimation_ListItem *animationItemFreeListPtr;
} EditorState;


static void loadImageStrip(Animation *animation, BackendRenderer *backendRenderer, char *filename_full_utf8, int widthPerImage) {
	Texture texOnStack = backendRenderer_loadFromFileToGPU(backendRenderer, filename_full_utf8);
	int count = 0;

	float xAt = 0;

	float widthTruncated = ((int)(texOnStack.width / widthPerImage))*widthPerImage;
	while(xAt < widthTruncated) {
		Texture *tex = pushStruct(&global_long_term_arena, Texture);
		easyPlatform_copyMemory(tex, &texOnStack, sizeof(Texture));

		tex->uvCoords.x = xAt / texOnStack.width;

		xAt += widthPerImage;

		tex->uvCoords.z = xAt / texOnStack.width;

		tex->aspectRatio_h_over_w = ((float)texOnStack.height) / ((float)(tex->uvCoords.z - tex->uvCoords.x)*(float)texOnStack.width);

		easyAnimation_pushFrame(animation, tex);

		count++;
	}
}

static void DEBUG_draw_stats(EditorState *editorState, Renderer *renderer, Font *font, float windowWidth, float windowHeight, float dt) {

	float16 orthoMatrix = make_ortho_matrix_top_left_corner(windowWidth, windowHeight, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);

	//NOTE: Draw the backing
	pushShader(renderer, &textureShader);
	float2 scale = make_float2(200, 400);
	// pushTexture(renderer, global_white_texture, make_float3(100, -200, 1.0f), scale, make_float4(0.3f, 0.3f, 0.3f, 1), make_float4(0, 0, 1, 1));
	///////////////////////////


	//NOTE: Draw the name of the file
	pushShader(renderer, &sdfFontShader);
		
	float fontScale = 0.6f;
	float4 color = make_float4(1, 1, 1, 1);

	float xAt = 0;
	float yAt = -1.5f*font->fontHeight*fontScale;

	float spacing = font->fontHeight*fontScale;

#define DEBUG_draw_stats_MACRO(title, size, draw_kilobytes) { char *name_str = 0; if(draw_kilobytes) { name_str = easy_createString_printf(&globalPerFrameArena, "%s  %d %dkilobytes", title, size, size/1000); } else { name_str = easy_createString_printf(&globalPerFrameArena, "%s  %d", title, size); } draw_text(renderer, font, name_str, xAt, yAt, fontScale, color); yAt -= spacing; }
#define DEBUG_draw_stats_FLOAT_MACRO(title, f0, f1) { char *name_str = 0; name_str = easy_createString_printf(&globalPerFrameArena, "%s  %f  %f", title, f0, f1); draw_text(renderer, font, name_str, xAt, yAt, fontScale, color); yAt -= spacing; }
	
	DEBUG_draw_stats_MACRO("Total Heap Allocated", global_debug_stats.total_heap_allocated, true);
	DEBUG_draw_stats_MACRO("Total Virtual Allocated", global_debug_stats.total_virtual_alloc, true);
	DEBUG_draw_stats_MACRO("Render Command Count", global_debug_stats.render_command_count, false);
	DEBUG_draw_stats_MACRO("Draw Count", global_debug_stats.draw_call_count, false);
	DEBUG_draw_stats_MACRO("Heap Block Count ", global_debug_stats.memory_block_count, false);
	DEBUG_draw_stats_MACRO("Per Frame Arena Total Size", DEBUG_get_total_arena_size(&globalPerFrameArena), true);

	// WL_Window *w = &editorState->windows[editorState->active_window_index];
	// DEBUG_draw_stats_FLOAT_MACRO("Start at: ", editorState->selectable_state.start_pos.x, editorState->selectable_state.start_pos.y);
	// DEBUG_draw_stats_FLOAT_MACRO("Target Scroll: ", w->scroll_target_pos.x, w->scroll_target_pos.y);

	DEBUG_draw_stats_FLOAT_MACRO("mouse scroll x ", global_platformInput.mouseX / windowWidth, global_platformInput.mouseY / windowHeight);
	DEBUG_draw_stats_FLOAT_MACRO("dt for frame ", dt, dt);

}


static EditorState *updateEditor(BackendRenderer *backendRenderer, float dt, float windowWidth, float windowHeight, bool should_save_settings, char *save_file_location_utf8_only_use_on_inititalize, Settings_To_Save save_settings_only_use_on_inititalize) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {

		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->mode = MODE_EDIT_BUFFER;

		#if DEBUG_BUILD
		editorState->font = initFont("..\\fonts\\liberation-mono.ttf");
		#else
		editorState->font = initFont(".\\fonts\\liberation-mono.ttf");
		#endif

		editorState->fontScale = 0.6f;

		editorState->draw_debug_memory_stats = false;

		srand(time(NULL));   // Initialization, should only be called once.
		
		editorState->player.velocity = make_float2(2, 0);
		editorState->player.pos = make_float3(0, 0, 10);
		editorState->playerTexture = backendRenderer_loadFromFileToGPU(backendRenderer, "..\\src\\images\\helicopter.png");

		editorState->pipeTexture =  backendRenderer_loadFromFileToGPU(backendRenderer, "..\\src\\images\\pipe.png");

		editorState->backgroundTexture = backendRenderer_loadFromFileToGPU(backendRenderer, "..\\src\\images\\backgroundCastles.png");

		for(int i = 0; i < arrayCount(editorState->rects); ++i) {
			Rect2f r = make_rect2f_center_dim(make_float2(i, 3), make_float2(1, 2));
			editorState->rects[i] = CollisionRect(r);
		}

		easyAnimation_initController(&editorState->playerAnimationController);
		easyAnimation_initAnimation(&editorState->playerIdleAnimation, "flappy_bird_idle");

		easyAnimation_addAnimationToController(&editorState->playerAnimationController, &editorState->animationItemFreeListPtr, &editorState->playerIdleAnimation, 0.08f);
		

		loadImageStrip(&editorState->playerIdleAnimation, backendRenderer, "..\\src\\images\\Flappy_bird.png", 64);

	} else {
		releaseMemoryMark(&global_perFrameArenaMark);
		global_perFrameArenaMark = takeMemoryMark(&globalPerFrameArena);
	}


	if(global_platformInput.keyStates[PLATFORM_KEY_F5].pressedCount > 0) {
		editorState->draw_debug_memory_stats = !editorState->draw_debug_memory_stats;
	}

	

	Renderer *renderer = &editorState->renderer;

	//NOTE: Clear the renderer out so we can start again
	clearRenderer(renderer);


	pushViewport(renderer, make_float4(0, 0, 0, 0));
	renderer_defaultScissors(renderer, windowWidth, windowHeight);
	pushClearColor(renderer, make_float4(0.9, 0.9, 0.9, 1));

	float2 mouse_point_top_left_origin = make_float2(global_platformInput.mouseX, global_platformInput.mouseY);	
	float2 mouse_point_top_left_origin_01 = make_float2(global_platformInput.mouseX / windowWidth, global_platformInput.mouseY / windowHeight);

	float fauxDimensionY = 1000;
	float fauxDimensionX = fauxDimensionY * (windowWidth/windowHeight);


	float16 orthoMatrix = make_ortho_matrix_origin_center(fauxDimensionX, fauxDimensionY, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix);
	

	pushShader(renderer, &rectOutlineShader);


	if(global_platformInput.keyStates[PLATFORM_KEY_UP].isDown) {
		editorState->player.pos.y += 0.1f;
		editorState->player.velocity.y = 0;

	} else {
		editorState->player.velocity.y -= 0.1f;
	}

	
	// if(global_platformInput.keyStates[PLATFORM_KEY_DOWN].isDown) {
		
	// }

	editorState->player.pos.xy = plus_float2(scale_float2(dt, editorState->player.velocity),  editorState->player.pos.xy);

	editorState->player.cameraPos.x = editorState->player.pos.x;

	pushShader(renderer, &textureShader);

	//NOTE: Background texture
	pushTexture(renderer, editorState->backgroundTexture.handle, make_float3(0, 0, 10), make_float2(fauxDimensionX, fauxDimensionY), make_float4(1, 1, 1, 1), make_float4(0, 0, 1, 1));

	float16 fovMatrix = make_perspective_matrix_origin_center(60.0f, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE, windowWidth / windowHeight);
	pushMatrix(renderer, fovMatrix);

	
	float2 playerSize = make_float2(1, 1);

	for(int i = 0; i < 128; ++i) {

		float yPos = 3.5f*perlin1d(i,  10, 10);


		Rect2f r = make_rect2f_center_dim(make_float2(i*2, yPos + 1.5f), make_float2(1, 2));

		float3 pos = {};
		pos.xy = get_centre_rect2f(r);
		pos.z = 9;

		//pushRect(renderer, minus_float3(pos, editorState->player.cameraPos), get_scale_rect2f(r), make_float4(1, 0, 0, 1));
		
		pushTexture(renderer, editorState->pipeTexture.handle, minus_float3(pos, editorState->player.cameraPos), get_scale_rect2f(r), make_float4(1, 1, 1, 1), make_float4(0, 0, 1, 1));
		

		Rect2f minowskiPlus = rect2f_minowski_plus(r, make_rect2f_center_dim(editorState->player.pos.xy, playerSize), pos.xy);
		if(in_rect2f_bounds(minowskiPlus, editorState->player.pos.xy)) {
			// editorState->player.pos.y = 0;
			// editorState->player.velocity.y = 0;
			//NOTE: Reset player
		}

		////////////
		/// This is the ones below
		/// 
		r = make_rect2f_center_dim(make_float2(i*2, yPos - 2.8), make_float2(1, 2));

		pos = {};
		pos.xy = get_centre_rect2f(r);
		pos.z = 9;

		//pushRect(renderer, minus_float3(pos, editorState->player.cameraPos), get_scale_rect2f(r), make_float4(1, 0, 0, 1));

		pushTexture(renderer, editorState->pipeTexture.handle, minus_float3(pos, editorState->player.cameraPos), get_scale_rect2f(r), make_float4(1, 1, 1, 1), make_float4(0, 0, 1, 1));

		minowskiPlus = rect2f_minowski_plus(r, make_rect2f_center_dim(editorState->player.pos.xy, playerSize), pos.xy);
		if(in_rect2f_bounds(minowskiPlus, editorState->player.pos.xy)) {
			// editorState->player.pos.y = 0;
			// editorState->player.velocity.y = 0;
			//NOTE: Reset player
		}
	}

	//NOTE: Draw player
		u64 start = __rdtsc();
		float16_multiply_SIMD(fovMatrix, float16_indentity());
		u64 end = __rdtsc();
		u64 resulta = end - start;

		start = __rdtsc();
		float16 playerMatrix = float16_multiply(fovMatrix, float16_angle_aroundZ(0.5f*HALF_PI32));
		end = __rdtsc();
		u64 resultb = end - start;

		pushMatrix(renderer, playerMatrix);

	Texture *t = easyAnimation_updateAnimation_getTexture(&editorState->playerAnimationController, &editorState->animationItemFreeListPtr, dt);
	
	pushTexture(renderer, t->handle, minus_float3(editorState->player.pos, editorState->player.cameraPos), playerSize, make_float4(1, 1, 1, 1), t->uvCoords);



	//NOTE: Draw the points
	float16 orthoMatrix1 = make_ortho_matrix_bottom_left_corner(fauxDimensionX, fauxDimensionY, MATH_3D_NEAR_CLIP_PlANE, MATH_3D_FAR_CLIP_PlANE);
	pushMatrix(renderer, orthoMatrix1);


	pushShader(renderer, &sdfFontShader);
	char *name_str = easy_createString_printf(&globalPerFrameArena, "%d points", editorState->points); 
	draw_text(renderer, &editorState->font, name_str, 50, 50, 1, make_float4(0, 0, 0, 1)); 


#if DEBUG_BUILD
	if(editorState->draw_debug_memory_stats) {
		renderer_defaultScissors(renderer, windowWidth, windowHeight);
		DEBUG_draw_stats(editorState, renderer, &editorState->font, windowWidth, windowHeight, dt);
	}
#endif

	return editorState;

}