void bumpMapping(sampler2D bumpMap, float bumpStep, mat3 normalMatrix, float bumpStrength, float bumpUVFactor, inout vec3 normal, inout vec2 uv) {
    float stp = bumpStep;

    float pxl = texture(bumpMap, vec2(uv.x-stp, uv.y)).r;
    float pxr = texture(bumpMap, vec2(uv.x+stp, uv.y)).r;
    float pxd = texture(bumpMap, vec2(uv.x, uv.y-stp)).r;
    float pxu = texture(bumpMap, vec2(uv.x, uv.y+stp)).r;

    float pxld = texture(bumpMap, vec2(uv.x-stp, uv.y-stp)).r;
    float pxrd = texture(bumpMap, vec2(uv.x+stp, uv.y-stp)).r;
    float pxlu = texture(bumpMap, vec2(uv.x-stp, uv.y+stp)).r;
    float pxru = texture(bumpMap, vec2(uv.x+stp, uv.y+stp)).r;

    vec2 dl = vec2(pxr - pxl, pxu - pxd);
    vec2 dld = vec2(pxru - pxld, pxlu - pxrd);

    const float halfisqrt2 = 0.353553390;

    dl.x += dld.x * halfisqrt2;
    dl.y += dld.x * halfisqrt2;

    dl.x -= dld.y * halfisqrt2;
    dl.y += dld.y * halfisqrt2;

    vec3 normalShift = normalMatrix * vec3(dl.x, dl.y, 0);
    normalShift *= bumpStrength;

    
    normal = normalize(normal - normalShift);
    uv = uv - dl * bumpUVFactor * bumpStrength;
}