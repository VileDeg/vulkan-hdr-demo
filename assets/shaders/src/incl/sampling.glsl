// 2x2
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
                    floor( (tc).x ) + 1, \
                    floor( (tc).y ) + 1  \
                ) \
            ), \
            imageLoad( \
                img, \
                ivec2( \
                    floor( (tc).x ), \
                    floor( (tc).y ) + 1 \
                ) \
            ), \
            fract( (tc).x ) \
        ),\
        fract( (tc).y )\
    )

// 4x4
#define quadlinear(img, tc) (\
    (\
        bilinear(img, vec2(tc) + vec2(-0.5, -0.5)) + \
        bilinear(img, vec2(tc) + vec2(-0.5,  0.5)) + \
        bilinear(img, vec2(tc) + vec2( 0.5, -0.5)) + \
        bilinear(img, vec2(tc) + vec2( 0.5,  0.5)) \
    ) * 0.25\
)


// 3x3 fetch
// A B C
// D E F
// G H I
#define f3x3tent(img, tc) ( \
    ( \
        imageLoad(img, (tc) + ivec2(-1, -1)) + \
        imageLoad(img, (tc) + ivec2(0 , -1)) * 2.0 + \
        imageLoad(img, (tc) + ivec2(1 , -1)) + \
        \
        imageLoad(img, (tc) + ivec2(-1,  0)) * 2.0 + \
        imageLoad(img, (tc)                ) * 4.0 + \
        imageLoad(img, (tc) + ivec2(1 ,  0)) * 2.0 + \
        \
        imageLoad(img, (tc) + ivec2(-1,  1)) + \
        imageLoad(img, (tc) + ivec2(0 ,  1)) * 2.0 + \
        imageLoad(img, (tc) + ivec2(1 ,  1)) \
    ) * 0.0625 /* 1/16 */\
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
        f3x3tent(img, (tc) + ivec2(0 , -3)) * 2.0 + \
        f3x3tent(img, (tc) + ivec2(3 , -3)) + \
        \
        f3x3tent(img, (tc) + ivec2(-3,  0)) * 2.0 + \
        f3x3tent(img, (tc)                ) * 4.0 + \
        f3x3tent(img, (tc) + ivec2(3 ,  0)) * 2.0 + \
        \
        f3x3tent(img, (tc) + ivec2(-3,  3)) + \
        f3x3tent(img, (tc) + ivec2(0 ,  3)) * 2.0 + \
        f3x3tent(img, (tc) + ivec2(3 ,  3)) \
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