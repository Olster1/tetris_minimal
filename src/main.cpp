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


typedef enum {
	SQUARE,
	LINE,
	J_BLOCK,
	L_BLOCK,
	Z_BLOCK,
	S_BLOCK,
	T_BLOCK,

	TETRIS_BLOCK_COUNT
} TetrisShapeType;

#define MAX_WINDOW_COUNT 8
#define MAX_BUFFER_COUNT 256 //TODO: Allow user to open unlimited buffers

#define BOARD_ROWS 20
#define BOARD_COLUMNS 10

typedef struct {
	TetrisShapeType type;
	float2 shape[4];
	bool active;

	float2 origin; //NOTE: USed to rotate shape around

	int rotation;

	u32 color;
} TetrisShape;


typedef enum {
	MODE_EDIT_BUFFER,
} EditorMode;

typedef struct {
	bool initialized;

	Renderer renderer;

	EditorMode mode;

	Font font;

	float fontScale;

	bool createShape;

	bool draw_debug_memory_stats;

	u32 board[BOARD_ROWS*BOARD_COLUMNS];

	float shapeTimer;

	TetrisShape currentShape;


	int points;
} EditorState;

static void clearBoard(EditorState *state) {
	for(int i = 0; i < BOARD_ROWS*BOARD_COLUMNS; ++i) {
		state->board[i] = 0;
	}

	state->points = 0;
}


static bool hasBlockOnBoard(EditorState *state, float2 pos) {
	if(state->board[((int)pos.y)*BOARD_COLUMNS + (int)pos.x] != 0) {
		return true;
	} else {
		return false;
	}
}

static void setBlockOnBoard(EditorState *state, float2 p, u32 value) {
	if(p.x >= 0 && p.x < BOARD_COLUMNS && p.y >= 0 && p.y < BOARD_ROWS) {
		state->board[((int)p.y)*BOARD_COLUMNS + (int)p.x] = value;
	}
	
}

static bool isOnBoard(float2 pos) {
	bool result = false;
	if(pos.x >= 0 && pos.x < BOARD_COLUMNS && pos.y >= 0 && pos.y < BOARD_ROWS) {
		result = true;
	}
	return result;
}

static void rotateShape(EditorState *editorState) {

	float2 positions[4] = {};

	editorState->currentShape.rotation++;

	if(editorState->currentShape.rotation >= 4) {
		editorState->currentShape.rotation = 0;
	}

	int rot = editorState->currentShape.rotation;


	//NOTE: Get rid of shape on board for simplicity sakes, will add it in at the end
	for(int i = 0; i < 4; ++i) {
		setBlockOnBoard(editorState, editorState->currentShape.shape[i], 0);
	}

	switch(editorState->currentShape.type) {
		case SQUARE: {
			//NOTE: Do nothing
			positions[0] = make_float2(0, 0);
			positions[1] = make_float2(1, 0);
			positions[2] = make_float2(0, 1);
			positions[3] = make_float2(1, 1);
		} break;
		case LINE: {

			if(rot == 0 || rot == 2) {
				positions[0] = make_float2(0, 0);
				positions[1] = make_float2(1, 0);
				positions[2] = make_float2(2, 0);
				positions[3] = make_float2(3, 0);
			} else {
				positions[0] = make_float2(1, -1);
				positions[1] = make_float2(1, 0);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(1, 2);
			}

			
		} break;
		case J_BLOCK: {
			if(rot == 0) {
				positions[0] = make_float2(0, 0);
				positions[1] = make_float2(0, 1);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(2, 1);
			} else if(rot == 1) {
				positions[0] = make_float2(1, 0);
				positions[1] = make_float2(2, 0);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(1, 2);
			} else if(rot == 2) {
				positions[0] = make_float2 (0, 1);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(2, 2);
			} else if(rot == 3) {
				positions[0] = make_float2(0, 2);
				positions[1] = make_float2(1, 2);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(1, 0);
			}

		} break;
		case L_BLOCK: {
			if(rot == 0) {
				positions[0] = make_float2(0, 1);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(2, 0);
			} else if(rot == 1) {
				positions[0] = make_float2(1, 0);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(1, 2);
				positions[3] = make_float2(2, 2);
			} else if(rot == 2) {
				positions[0] = make_float2 (0, 1);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(0, 2);
			} else if(rot == 3) {
				positions[0] = make_float2(0, 0);
				positions[1] = make_float2(1, 0);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(1, 2);
			}
			

		} break;
		case Z_BLOCK: {
			if(rot == 0) {
				positions[0] = make_float2(0, 0);
				positions[1] = make_float2(1, 0);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(2, 1);
			} else if(rot == 1) {
				positions[0] = make_float2(2, 0);
				positions[1] = make_float2(2, 1);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(1, 2);
			} else if(rot == 2) {
				positions[0] = make_float2 (0, 1);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(1, 2);
				positions[3] = make_float2(2, 2);
			} else if(rot == 3) {
				positions[0] = make_float2(1, 0);
				positions[1] = make_float2(0, 1);
				positions[2] = make_float2(0, 2);
				positions[3] = make_float2(1, 1);
			}
		} break;
		case S_BLOCK: {
			if(rot == 0) {
				positions[0] = make_float2(0, 1);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(1, 0);
				positions[3] = make_float2(2, 0);

			} else if(rot == 1) {
				positions[0] = make_float2(1, 0);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(2, 2);

			} else if(rot == 2) {
				//NOTE: HERE
				positions[0] = make_float2(0, 2);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(1, 2);
			} else if(rot == 3) {
				positions[0] = make_float2(0, 0);
				positions[1] = make_float2(0, 1);
				positions[2] = make_float2(1, 1);
				positions[3] = make_float2(1, 2);
			}

		} break;
		case T_BLOCK: {
			if(rot == 0) {
				positions[0] = make_float2(0, 1);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(1, 0);

			} else if(rot == 1) {
				positions[0] = make_float2(1, 0);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(1, 2);

			} else if(rot == 2) {
				positions[0] = make_float2(0, 1);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(2, 1);
				positions[3] = make_float2(1, 2);
			} else if(rot == 3) {
				positions[0] = make_float2(1, 0);
				positions[1] = make_float2(1, 1);
				positions[2] = make_float2(0, 1);
				positions[3] = make_float2(1, 2);
			}
			
		} break;	
	}

	bool ok = true;
	float2 origin = editorState->currentShape.origin;
	for(int i = 0; i < 4 && ok; ++i) {
		float2 pos = make_float2(origin.x + positions[i].x, origin.y - positions[i].y);

		if(!isOnBoard(pos) || hasBlockOnBoard(editorState, pos)) {
			ok = false;
			break;
		} 
	}

	//NOTE: Put shape back
	if(ok) {
		for(int i = 0; i < 4; ++i) {
			float2 pos = make_float2(origin.x + positions[i].x, origin.y - positions[i].y);

			editorState->currentShape.shape[i] = pos;

			setBlockOnBoard(editorState, pos, editorState->currentShape.color);
		}
	} else {
		for(int i = 0; i < 4; ++i) {
			setBlockOnBoard(editorState, editorState->currentShape.shape[i], editorState->currentShape.color);
		}
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


static void drawBoard(EditorState *editorState, Renderer *renderer, int drawPass) {


	if(drawPass == 0) {
		pushShader(renderer, &textureShader);
	} else if(drawPass == 1) {
		pushShader(renderer, &rectOutlineShader);
	}
	//NOTE: Draw board in two passes for the renderer because it uses batch rendering as it comes so want it to make sure we render all in a batch
	for(int y = 0; y < BOARD_ROWS; y++) {
		for(int x = 0; x < BOARD_COLUMNS; x++) {

			
			float2 scale = make_float2(40, 40);
			float2 centre = make_float2(x*scale.x - 0.5f*BOARD_COLUMNS*scale.x, y*scale.y - 0.5f*BOARD_ROWS*scale.y);
			

			if(drawPass == 0) {
				if(editorState->board[y*BOARD_COLUMNS + x] != 0) 
				{
					u32 c = editorState->board[y*BOARD_COLUMNS + x];
					float4 color = color_hexARGBTo01(c);
					
					pushTexture(renderer, global_white_texture, make_float3(centre.x, centre.y, 1.0f), scale, color, make_float4(0, 1, 0, 1));

				}
			} else if(drawPass == 1) {

				pushRectOutline(renderer, make_float3(centre.x, centre.y, 0.8f), scale, make_float4(1, 0, 0, 1));
			}

		}
	}
}

static bool posInShape(EditorState *state, float2 p) {
	bool result = false;
	for(int i = 0; i < 4; ++i) {
		float2 *pos = &state->currentShape.shape[i];

		if(pos->x == p.x && pos->y == p.y) {
			result = true;
		}
	}
	return result;
}


static EditorState *updateEditor(float dt, float windowWidth, float windowHeight, bool should_save_settings, char *save_file_location_utf8_only_use_on_inititalize, Settings_To_Save save_settings_only_use_on_inititalize) {
	EditorState *editorState = (EditorState *)global_platform.permanent_storage;
	assert(sizeof(EditorState) < global_platform.permanent_storage_size);
	if(!editorState->initialized) {

		editorState->initialized = true;

		initRenderer(&editorState->renderer);

		editorState->mode = MODE_EDIT_BUFFER;

		clearBoard(editorState);

		#if DEBUG_BUILD
		editorState->font = initFont("..\\fonts\\liberation-mono.ttf");
		#else
		editorState->font = initFont(".\\fonts\\liberation-mono.ttf");
		#endif

		editorState->fontScale = 0.6f;

		editorState->draw_debug_memory_stats = false;

		editorState->createShape = true;

		editorState->currentShape.active = false;


		srand(time(NULL));   // Initialization, should only be called once.
		
		

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

if(editorState->createShape) {


	editorState->currentShape.type = (TetrisShapeType)(int)(rand() % TETRIS_BLOCK_COUNT);     
	editorState->currentShape.rotation = 0;
	float2 positions[4];

	switch(editorState->currentShape.type) {
		case SQUARE: {
			positions[0] = make_float2(0, 0);
			positions[1] = make_float2(1, 0);
			positions[2] = make_float2(0, 1);
			positions[3] = make_float2(1, 1);

			editorState->currentShape.color = 0xFFFFFF00;
		} break;
		case LINE: {
			positions[0] = make_float2(0, 0);
			positions[1] = make_float2(1, 0);
			positions[2] = make_float2(2, 0);
			positions[3] = make_float2(3, 0);

			editorState->currentShape.color = 0xFF00FFFF;
		} break;
		case J_BLOCK: {
			positions[0] = make_float2(0, 0);
			positions[1] = make_float2(0, 1);
			positions[2] = make_float2(1, 1);
			positions[3] = make_float2(2, 1);

			editorState->currentShape.color = 0xFFFF00FF;
		} break;
		case L_BLOCK: {
			positions[0] = make_float2(0, 1);
			positions[1] = make_float2(1, 1);
			positions[2] = make_float2(2, 1);
			positions[3] = make_float2(2, 0);

			editorState->currentShape.color = 0xFF8000FF;
		} break;
		case Z_BLOCK: {
			positions[0] = make_float2(0, 0);
			positions[1] = make_float2(1, 0);
			positions[2] = make_float2(1, 1);
			positions[3] = make_float2(2, 1);

			editorState->currentShape.color = 0xFF800000;

		} break;
		case S_BLOCK: {
			positions[0] = make_float2(0, 1);
			positions[1] = make_float2(1, 1);
			positions[2] = make_float2(1, 0);
			positions[3] = make_float2(2, 0);

			editorState->currentShape.color = 0xFF1000FF;


		} break;
		case T_BLOCK: {
			positions[0] = make_float2(0, 1);
			positions[1] = make_float2(1, 1);
			positions[2] = make_float2(2, 1);
			positions[3] = make_float2(1, 0);

			editorState->currentShape.color = 0xFF80FF00;
		} break;	
	} 

	for(int i = 0; i < 4; ++i) {

		float2 p = positions[i]; 
		float2 pos = make_float2(p.x, BOARD_ROWS-1-p.y);

		if(hasBlockOnBoard(editorState, pos)) {
			//NOTE: Game over

			clearBoard(editorState);
		} 

		
		editorState->currentShape.shape[i] = pos;
		
	}

	editorState->currentShape.origin = make_float2(0, BOARD_ROWS-1);

	//NOTE: Now add the actual shape so it doesn't get deleted

	for(int i = 0; i < 4; ++i) {

		float2 pos = editorState->currentShape.shape[i];

		editorState->board[(int)pos.y*BOARD_COLUMNS + (int)pos.x] = editorState->currentShape.color;
	}

	editorState->currentShape.active = true;
	editorState->createShape = false;

	editorState->shapeTimer = 0;
}

float speedFactor = 1.0f;

if(global_platformInput.keyStates[PLATFORM_KEY_DOWN].isDown) {
	speedFactor = 10.0f;
}



if(editorState->currentShape.active) {
	editorState->shapeTimer += dt*speedFactor;

	TetrisShape tempShape = editorState->currentShape;


	if(global_platformInput.keyStates[PLATFORM_KEY_SPACE].pressedCount > 0) {
		//NOTE: Rotate shape

		rotateShape(editorState);
	}

	if(global_platformInput.keyStates[PLATFORM_KEY_LEFT].pressedCount > 0) {

		bool canMove = true;
		for(int i = 0; i < 4 && canMove; ++i) {
			float2 pos = editorState->currentShape.shape[i];

			pos.x--;

			if(pos.x >= 0) {
				if(hasBlockOnBoard(editorState, pos) && !posInShape(editorState, pos)) {
					canMove = false;
				}

			} else {
				canMove = false;
			}
		}

		if(canMove) {
			editorState->currentShape.origin.x--;

			for(int i = 0; i < 4; ++i) {
				float2 pos = editorState->currentShape.shape[i];
				editorState->board[((int)pos.y)*BOARD_COLUMNS + (int)pos.x] = 0;

				editorState->currentShape.shape[i].x--;
				pos = editorState->currentShape.shape[i];

				editorState->currentShape.shape[i] = make_float2(pos.x, pos.y);

			}

			for(int i = 0; i < 4; ++i) {
				float2 pos = editorState->currentShape.shape[i];
				editorState->board[((int)pos.y)*BOARD_COLUMNS + (int)pos.x] = editorState->currentShape.color;

			}

		} else {
			editorState->currentShape = tempShape;
		}
	
	}

	if(global_platformInput.keyStates[PLATFORM_KEY_RIGHT].pressedCount > 0) {

		bool canMove = true;
		for(int i = 0; i < 4 && canMove; ++i) {
			float2 pos = editorState->currentShape.shape[i];

			pos.x++;

			if(pos.x < BOARD_COLUMNS) {
				if(editorState->board[((int)pos.y)*BOARD_COLUMNS + (int)pos.x] != 0 && !posInShape(editorState, pos)) {
					canMove = false;
				}

			} else {
				canMove = false;
			}
		}

		if(canMove) {

			editorState->currentShape.origin.x++;
			for(int i = 0; i < 4; ++i) {
				float2 *pos = &editorState->currentShape.shape[i];
				editorState->board[((int)pos->y)*BOARD_COLUMNS + (int)pos->x] = 0;

				pos->x++;

				editorState->currentShape.shape[i] = *pos;

			}

			for(int i = 0; i < 4; ++i) {
				float2 *pos = &editorState->currentShape.shape[i];
				editorState->board[((int)pos->y)*BOARD_COLUMNS + (int)pos->x] = editorState->currentShape.color;

			}

		} else {
			editorState->currentShape = tempShape;
		}
	
	}

	if(editorState->shapeTimer > 1) {

	// if(global_platformInput.keyStates[PLATFORM_KEY_DOWN].pressedCount > 0) {

		for(int i = 0; i < 4; ++i) {
			float2 pos = editorState->currentShape.shape[i];
			editorState->board[((int)pos.y)*BOARD_COLUMNS + (int)pos.x] = 0;
		}

		TetrisShape tempShape = editorState->currentShape;

		float2 tempOrigin = editorState->currentShape.origin;

		editorState->currentShape.origin.y--;

		bool stopped = false;

		for(int i = 0; i < 4 && editorState->currentShape.active; ++i) {

			editorState->currentShape.shape[i].y--;

			if(hasBlockOnBoard(editorState, editorState->currentShape.shape[i]) || editorState->currentShape.shape[i].y < 0) {
				editorState->currentShape.active = false;
				editorState->createShape = true;

				//NOTE: Reset the positions
				editorState->currentShape = tempShape;

				editorState->currentShape.origin = tempOrigin;

				stopped = true;

				break;
			}

		}



		for(int i = 0; i < 4; ++i) {
			float2 pos = editorState->currentShape.shape[i];
			editorState->board[((int)pos.y)*BOARD_COLUMNS + (int)pos.x] = editorState->currentShape.color;

			editorState->currentShape.shape[i] = make_float2(pos.x, pos.y);
		}


		//check win state
		if(stopped) {
			for(int i = 0; i < BOARD_ROWS; ) {

				int inc = 1;
				int winCount = 0;
				for(int j = 0; j < BOARD_COLUMNS; ++j) {
					u32 v = editorState->board[((int)i)*BOARD_COLUMNS + (int)j];

					if(v != 0) {
						winCount++;
					} else {
						break;
					}
				}

				if(winCount == BOARD_COLUMNS) {

					editorState->points += 10;
					inc = 0;
					//NOTE: remove row
					for(int y = i; y < BOARD_ROWS; ++y) {
						int winCount = 0;
						for(int x = 0; x < BOARD_ROWS; ++x) {
							if(y < (BOARD_ROWS - 1)) {
								//NOTE: Get value from above row
								u32 v = editorState->board[((int)y + 1)*BOARD_COLUMNS + (int)x];	
								editorState->board[((int)y)*BOARD_COLUMNS + (int)x] = v;	
							} else {
								editorState->board[((int)y)*BOARD_COLUMNS + (int)x] = 0;	
							}
							
						}
					}
				}

				i += inc;
			}
		}

		editorState->shapeTimer = 0;
	}
}

	

	
	drawBoard(editorState, renderer, 0);
	drawBoard(editorState, renderer, 1);


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