ivec2 borderClamp(ivec2 coords, ivec2 dim) {
	    coords = clamp(coords, ivec2(0), dim-ivec2(1));
	    return coords;
    }

ivec2 borderWrap(ivec2 coords, ivec2 dim) {
	if (coords.x > dim.x-1) {
		coords.x -= dim.x;
	}
	if (coords.y > dim.y-1) {
		coords.y -= dim.y;
	}
	if (coords.x < 0) {
		coords.x += dim.x-1;
	}
	if (coords.y < 0) {
		coords.y += dim.y-1;
	}
	return coords;
}