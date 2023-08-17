
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