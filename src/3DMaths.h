#include <float.h> //NOTE: For FLT_MAX

#define PI32 3.14159265358979
#define SIN45 0.70710678118
#define TAU32 6.283185307
#define HALF_PI32 0.5f*PI32

static inline float get_abs_value(float value) {
	if(value < 0) {
		value *= -1.0f;
	}
	return value;
}


inline float ATan2_0toTau(float Y, float X) {
    float Result = (float)atan2(Y, X);
    if(Result < 0) {
        Result += TAU32; // is in the bottom range ie. 180->360. -PI32 being PI32. So we can flip it up by adding TAU32
    }
    
    assert(Result >= 0 && Result <= (TAU32 + 0.00001));
    return Result;
}


struct float2
{
    float x, y;
};

struct float3
{
	union {
		struct 
		{
			float x, y, z;	
		};
		struct 
		{
			float2 xy;
			float z;	
		};
    	
	};
}; 

struct float4
{
    float x, y, z, w;
}; 

static float2 make_float2(float x0, float y0) {
	float2 result = {};

	result.x = x0;
	result.y = y0;

	return result;
}

static float2 scale_float2(float dt, float2 value) {
	return make_float2(dt*value.x, dt*value.y);
}

static float2 plus_float2(float2 a, float2 b) {
	return make_float2(a.x+b.x, a.y+b.y);
}


static float2 lerp_float2(float2 a, float2 b, float t) {
	return make_float2((b.x - a.x)*t + a.x, (b.y - a.y)*t + a.y);
}

static float3 make_float3(float x0, float y0, float z0) {
	float3 result = {};

	result.x = x0;
	result.y = y0;
	result.z = z0;

	return result;
}

static float3 minus_float3(float3 a, float3 b) {
	return make_float3(a.x-b.x, a.y-b.y, a.z-b.z);
}

static float4 make_float4(float x, float y, float z, float w) {
	float4 result = {};

	result.x = x;
	result.y = y;
	result.z = z;
	result.w = w;

	return result;
}

struct Rect2f {
	float minX;
	float minY;
	float maxX;
	float maxY;
};

static Rect2f make_rect2f(float minX, float minY, float maxX, float maxY) {
	Rect2f result = {};

	result.minX = minX;
	result.minY = minY;
	result.maxX = maxX;
	result.maxY = maxY;

	return result; 
}

static Rect2f make_rect2f_center_dim(float2 centre, float2 dim) {
	Rect2f result = {};

	result.minX = centre.x - 0.5f*dim.x;
	result.minY = centre.y - 0.5f*dim.y;
	result.maxX = centre.x + 0.5f*dim.x;
	result.maxY = centre.y + 0.5f*dim.y;

	return result; 
}

static float2 get_centre_rect2f(Rect2f r) {
	float2 result = {};

	result.x = 0.5f*(r.maxX - r.minX) + r.minX;
	result.y = 0.5f*(r.maxY - r.minY) + r.minY;

	return result;
}

static float2 get_scale_rect2f(Rect2f r) {
	float2 result = {};

	result.x = (r.maxX - r.minX);
	result.y = (r.maxY - r.minY);

	return result;
}

static Rect2f rect2f_union(Rect2f a, Rect2f b) {
	Rect2f result = a;

	if(b.minX < a.minX) {
		result.minX = b.minX;
	}

	if(b.minY < a.minY) {
		result.minY = b.minY;
	}

	if(b.maxX > a.maxX) {
		result.maxX = b.maxX;
	}

	if(b.maxY > a.maxY) {
		result.maxY = b.maxY;
	}

	return result;
}

static Rect2f rect2f_minowski_plus(Rect2f a, Rect2f b, float2 center) {
	float2 a_ = get_scale_rect2f(a);
	float2 b_ = get_scale_rect2f(b);

	float2 scale = plus_float2(a_, b_);

	Rect2f result = make_rect2f_center_dim(center, scale);

	return result;
}	


struct float16
{
	union {
		struct {
			float E[16];
		};
		struct {
			float E_[4][4];
		};
	};
    
}; 

#define MATH_3D_NEAR_CLIP_PlANE 0.1f
#define MATH_3D_FAR_CLIP_PlANE 1000.0f

static float16 make_ortho_matrix_bottom_left_corner(float planeWidth, float planeHeight, float nearClip, float farClip) {
	//NOTE: The size of the plane we're projection onto
	float a = 2.0f / planeWidth;
	float b = 2.0f / planeHeight;

	//NOTE: We can offset the origin of the viewport by adding these to the translation part of the matrix
	float originOffsetX = -1; //NOTE: Defined in NDC space
	float originOffsetY = -1; //NOTE: Defined in NDC space


	float16 result = {{
	        a, 0, 0, 0,
	        0, b, 0, 0,
	        0, 0, 1.0f/(farClip - nearClip), 0,
	        originOffsetX, originOffsetY, nearClip/(nearClip - farClip), 1
	    }};

	return result;
}

static float16 make_ortho_matrix_top_left_corner(float planeWidth, float planeHeight, float nearClip, float farClip) {
	//NOTE: The size of the plane we're projection onto
	float a = 2.0f / planeWidth;
	float b = 2.0f / planeHeight;

	//NOTE: We can offset the origin of the viewport by adding these to the translation part of the matrix
	float originOffsetX = -1; //NOTE: Defined in NDC space
	float originOffsetY = 1; //NOTE: Defined in NDC space


	float16 result = {{
	        a, 0, 0, 0,
	        0, b, 0, 0,
	        0, 0, 1.0f/(farClip - nearClip), 0,
	        originOffsetX, originOffsetY, nearClip/(nearClip - farClip), 1
	    }};

	return result;
}

static float16 make_ortho_matrix_origin_center(float planeWidth, float planeHeight, float nearClip, float farClip) {
	//NOTE: The size of the plane we're projection onto
	float a = 2.0f / planeWidth;
	float b = 2.0f / planeHeight;

	//NOTE: We can offset the origin of the viewport by adding these to the translation part of the matrix
	float originOffsetX = 0; //NOTE: Defined in NDC space
	float originOffsetY = 0; //NOTE: Defined in NDC space


	float16 result = {{
	        a, 0, 0, 0,
	        0, b, 0, 0,
	        0, 0, 1.0f/(farClip - nearClip), 0,
	        originOffsetX, originOffsetY, nearClip/(nearClip - farClip), 1
	    }};

	return result;
}

static float16 make_perspective_matrix_origin_center(float FOV_degrees, float nearClip, float farClip, float aspectRatio_x_over_y) {
	//NOTE: Convert the Camera's Field of View from Degress to Radians
	float FOV_radians = (FOV_degrees*PI32) / 180.0f;

	//NOTE: Get the size of the plane the game world will be projected on.
	float t = tan(FOV_radians/2); //plane's height
	float r = t*aspectRatio_x_over_y; //plane's width

	float16 result = {{
	        1 / r, 0, 0, 0,
	        0, 1 / t, 0, 0,
	        0, 0, farClip/(farClip - nearClip), 1,
	        0, 0, (-nearClip*farClip)/(farClip - nearClip), 0
	    }};

	    return result;
}


static bool in_rect2f_bounds(Rect2f bounds, float2 point) {
	bool result = false;

	if(point.x >= bounds.minX && point.x < bounds.maxX && point.y >= bounds.minY && point.y < bounds.maxY) {
		result = true;
	}

	return result;
}

static float16 float16_indentity() {
	float16 result = {};

	result.E_[0][0] = 1;
	result.E_[1][1] = 1;
	result.E_[2][2] = 1;
	result.E_[3][3] = 1;

	return result;
}

float16 float16_angle_aroundZ(float angle_radians) {
    float16 result = {{
            (float)cos(angle_radians), (float)sin(angle_radians), 0, 0,
            (float)cos(angle_radians + HALF_PI32), (float)sin(angle_radians + HALF_PI32), 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1
        }};
    return result;
}


// https://codereview.stackexchange.com/questions/101144/simd-matrix-multiplication

//NOTE: This is actually slower, we are still doing 16 loops whereas we should only have to do 4 
static float16 float16_multiply_SIMD(float16 a, float16 b) { //NOTE: This is actually slower than one below
	float16 result = {};

	for(int i = 0; i < 4; ++i) {
        for(int j = 0; j < 4; ++j) {

        	__m128 	a_ = _mm_set_ps(a.E_[0][j], a.E_[1][j], a.E_[2][j], a.E_[3][j]);

        	__m128 	b_ = _mm_set_ps(b.E_[i][0], b.E_[i][1], b.E_[i][2], b.E_[i][3]);

        	__m128 c_ = _mm_mul_ps(a_, b_);

        	//NOTE: This sums each 32bit float tp get one value
        	c_ = _mm_hadd_ps(c_, c_);
        	c_ = _mm_hadd_ps(c_, c_);

        	float ret[4];

            _mm_storeu_ps(ret, c_);
            
            result.E_[i][j] = ret[0];
            
        }
    }

    return result;
}

float16 float16_multiply(float16 a, float16 b) {
    float16 result = {};
    
    for(int i = 0; i < 4; ++i) {
        for(int j = 0; j < 4; ++j) {
            
            result.E_[i][j] = 
                a.E_[0][j] * b.E_[i][0] + 
                a.E_[1][j] * b.E_[i][1] + 
                a.E_[2][j] * b.E_[i][2] + 
                a.E_[3][j] * b.E_[i][3];
            
        }
    }
    
    return result;
}
