uniform vec2 wireAlpha = vec2(1.0, 0.0);
uniform float wireFadeDepthBias = 0.0;

float wire_alpha(float depth)
{
	float alpha = wireAlpha.x;

	if (wireAlpha.y > 0) {
		float view_z = -get_view_z_from_depth(depth);

		/* Clamp bias by the near clip plane. */
		float bias_clamp = max(-get_view_z_from_depth(0), wireFadeDepthBias);

		/* Subtract bias from the depth and compute the fade alpha. */
		alpha *= pow(0.5, max(0.0, view_z - bias_clamp) * wireAlpha.y);
	}

	return clamp(alpha, 0, 1);
}
