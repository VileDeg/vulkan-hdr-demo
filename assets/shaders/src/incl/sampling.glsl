// 2x2 billinear filtering for image2D
#define bilinear(img, tc) \
    mix( \
        mix( \
            imageLoad( \
                img, \
                ivec2( \
                    floor( (tc).x ), \
                    floor( (tc).y )  \
                ) \
            ), \
            imageLoad( \
                img, \
                ivec2( \
                    floor( (tc).x ) + 1, \
                    floor( (tc).y ) \
                ) \
            ), \
            fract( (tc).x ) \
        ), \
        mix( \
            imageLoad( \
                img, \
                ivec2( \
                    floor( (tc).x ), \
                    floor( (tc).y ) + 1 \
                ) \
            ), \
            imageLoad(\
                img, \
                ivec2(\
                    floor((tc).x) + 1, \
                    floor((tc).y) + 1  \
                ) \
            ), \
            fract( (tc).x ) \
        ), \
        fract( (tc).y ) \
    )

// wut ?
#define quadlinear(img, tc) (\
    (\
        bilinear(img, vec2(tc) + vec2(-0.5, -0.5)) + \
        bilinear(img, vec2(tc) + vec2(-0.5,  0.5)) + \
        bilinear(img, vec2(tc) + vec2( 0.5, -0.5)) + \
        bilinear(img, vec2(tc) + vec2( 0.5,  0.5)) \
    ) * 0.25\
)

// Image2D implementation of sampling method proposed in COD: Advanced Warface 2014 presentation
#define f13bilinear(_img, _tc) \
    vec4 _a = bilinear(_img, (_tc) + vec2(-1.5, -1.5)); \
    vec4 _b = bilinear(_img, (_tc) + vec2( 0.5, -1.5)); \
    vec4 _c = bilinear(_img, (_tc) + vec2( 2.5, -1.5)); \
    \
    vec4 _d = bilinear(_img, (_tc) + vec2(-1.5,  0.5)); \
    vec4 _e = bilinear(_img, (_tc) + vec2( 0.5,  0.5)); \
    vec4 _f = bilinear(_img, (_tc) + vec2( 2.5,  0.5)); \
    \
    vec4 _g = bilinear(_img, (_tc) + vec2(-1.5, -2.5)); \
    vec4 _h = bilinear(_img, (_tc) + vec2( 0.5, -2.5)); \
    vec4 _i = bilinear(_img, (_tc) + vec2( 2.5, -2.5)); \
    \
    vec4 _j = bilinear(_img, (_tc) + vec2(-0.5, -0.5)); \
    vec4 _k = bilinear(_img, (_tc) + vec2( 1.5, -0.5)); \
    vec4 _l = bilinear(_img, (_tc) + vec2(-0.5,  1.5)); \
    vec4 _m = bilinear(_img, (_tc) + vec2( 1.5,  1.5)); \
    \
    vec4 _f13bilinearOut  =  _e          *0.125  ; \
	     _f13bilinearOut += (_a+_c+_g+_i)*0.03125; \
	     _f13bilinearOut += (_b+_d+_f+_h)*0.0625 ; \
	     _f13bilinearOut += (_j+_k+_l+_m)*0.125  ; \

// 3x3 fetch
// 1 2 1
// 2 4 2
// 1 2 1
#define f3x3tent(img, tc) ( \
    ( \
        imageLoad(img, (tc) + ivec2(-1, -1)) + \
        imageLoad(img, (tc) + ivec2( 0, -1)) * 2.0 + \
        imageLoad(img, (tc) + ivec2( 1, -1)) + \
        \
        imageLoad(img, (tc) + ivec2(-1,  0)) * 2.0 + \
        imageLoad(img, (tc)                ) * 4.0 + \
        imageLoad(img, (tc) + ivec2( 1,  0)) * 2.0 + \
        \
        imageLoad(img, (tc) + ivec2(-1,  1)) + \
        imageLoad(img, (tc) + ivec2( 0,  1)) * 2.0 + \
        imageLoad(img, (tc) + ivec2( 1,  1)) \
    ) * 0.0625 /* 1/16 */\
)

#define f3x3tent_r(img, tc, r) ( \
    ( \
        bilinear(img, (tc) + ivec2(-1, -1) * r)       + \
        bilinear(img, (tc) + ivec2( 0, -1) * r) * 2.0 + \
        bilinear(img, (tc) + ivec2( 1, -1) * r)       + \
        \
        bilinear(img, (tc) + ivec2(-1,  0) * r) * 2.0 + \
        bilinear(img, (tc)                    ) * 4.0 + \
        bilinear(img, (tc) + ivec2( 1,  0) * r) * 2.0 + \
        \
        bilinear(img, (tc) + ivec2(-1,  1) * r)       + \
        bilinear(img, (tc) + ivec2( 0,  1) * r) * 2.0 + \
        bilinear(img, (tc) + ivec2( 1,  1) * r) \
    ) * 0.0625 /* 1/16 */\
)




// W0 + W1 * 4 + W2 * 8 + W3 * 8 + W4 * 4

// 9x9
// . . .  . . .  . . .
// . A .  . B .  . C .
// . . .  . . .  . . .
//
// . . .  . . .  . . .
// . D .  . E .  . F .
// . . .  . . .  . . .
//
// . . .  . . .  . . .
// . G .  . H .  . I .
// . . .  . . .  . . .
#define f9x9tent(img, tc) ( \
    ( \
        f3x3tent(img, (tc) + ivec2(-3, -3)) + \
        f3x3tent(img, (tc) + ivec2( 0, -3)) * 2.0 + \
        f3x3tent(img, (tc) + ivec2( 3, -3)) + \
        \
        f3x3tent(img, (tc) + ivec2(-3,  0)) * 2.0 + \
        f3x3tent(img, (tc)                ) * 4.0 + \
        f3x3tent(img, (tc) + ivec2( 3,  0)) * 2.0 + \
        \
        f3x3tent(img, (tc) + ivec2(-3,  3)) + \
        f3x3tent(img, (tc) + ivec2( 0,  3)) * 2.0 + \
        f3x3tent(img, (tc) + ivec2( 3,  3)) \
    ) * 0.0625 /* 1/16 */\
)

#define f9x9tent_r(img, tc, r) ( \
    ( \
        f3x3tent_r(img, (tc) + ivec2(-3, -3) * r, r) + \
        f3x3tent_r(img, (tc) + ivec2( 0, -3) * r, r) * 2.0 + \
        f3x3tent_r(img, (tc) + ivec2( 3, -3) * r, r) + \
        \
        f3x3tent_r(img, (tc) + ivec2(-3,  0) * r, r) * 2.0 + \
        f3x3tent_r(img, (tc)                    , r) * 4.0 + \
        f3x3tent_r(img, (tc) + ivec2( 3,  0) * r, r) * 2.0 + \
        \
        f3x3tent_r(img, (tc) + ivec2(-3,  3) * r, r) + \
        f3x3tent_r(img, (tc) + ivec2( 0,  3) * r, r) * 2.0 + \
        f3x3tent_r(img, (tc) + ivec2( 3,  3) * r, r) \
    ) * 0.0625 /* 1/16 */\
)


// 9x9
// . . .  . . .  . . .
// . . .  . A .  . . .
// . . .  . . .  . . .
//
// . . .  . . .  . . .
// . B .  . C .  . D .
// . . .  . . .  . . .
//
// . . .  . . .  . . .
// . . .  . E .  . . .
// . . .  . . .  . . .
#define f9x9tent_reduced(img, tc) ( \
    ( \
        f3x3tent(img, (tc) + ivec2(0 , -3)) + \
        \
        f3x3tent(img, (tc) + ivec2(-3,  0)) + \
        f3x3tent(img, (tc)                ) * 2.0 + \
        f3x3tent(img, (tc) + ivec2(3 ,  0)) + \
        \
        f3x3tent(img, (tc) + ivec2(0 ,  3)) \
    ) * 0.16666 /* 1/6 */\
)




#define H 4

#define W0 (1  << H)
#define W1 (W0 >> 1)
#define W2 (W0 >> 2)
#define W3 (W0 >> 3)
#define W4 (W0 >> 4)

// 5x5 fetch
#define f5x5tent(img, tc) ( \
    ( \
        imageLoad(img, (tc) + ivec2(-2, -2)) * W4 + \
        imageLoad(img, (tc) + ivec2(-1, -2)) * W3 + \
        imageLoad(img, (tc) + ivec2( 0, -2)) * W2 + \
        imageLoad(img, (tc) + ivec2( 1, -2)) * W3 + \
        imageLoad(img, (tc) + ivec2( 2, -2)) * W4 + \
        \
        imageLoad(img, (tc) + ivec2(-2, -1)) * W3 + \
        imageLoad(img, (tc) + ivec2(-1, -1)) * W2 + \
        imageLoad(img, (tc) + ivec2( 0, -1)) * W1 + \
        imageLoad(img, (tc) + ivec2( 1, -1)) * W2 + \
        imageLoad(img, (tc) + ivec2( 2, -1)) * W3 + \
        \
        imageLoad(img, (tc) + ivec2(-2,  0)) * W2 + \
        imageLoad(img, (tc) + ivec2(-1,  0)) * W1 + \
        imageLoad(img, (tc) + ivec2( 0,  0)) * W0 + \
        imageLoad(img, (tc) + ivec2( 1,  0)) * W1 + \
        imageLoad(img, (tc) + ivec2( 2,  0)) * W2 + \
        \
        imageLoad(img, (tc) + ivec2(-2,  1)) * W3 + \
        imageLoad(img, (tc) + ivec2(-1,  1)) * W2 + \
        imageLoad(img, (tc) + ivec2( 0,  1)) * W1 + \
        imageLoad(img, (tc) + ivec2( 1,  1)) * W2 + \
        imageLoad(img, (tc) + ivec2( 2,  1)) * W3 + \
        \
        imageLoad(img, (tc) + ivec2(-2,  2)) * W4 + \
        imageLoad(img, (tc) + ivec2(-1,  2)) * W3 + \
        imageLoad(img, (tc) + ivec2( 0,  2)) * W2 + \
        imageLoad(img, (tc) + ivec2( 1,  2)) * W3 + \
        imageLoad(img, (tc) + ivec2( 2,  2)) * W4 \
    ) * 0.007575 \
)