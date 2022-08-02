#version 150

in vec3 position;
in vec4 color;
in vec3 p_left, p_right, p_up, p_down;
out vec4 col;

uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;
uniform int mode;

void main()
{
  // mode for point(1), line(2), and triangle(3) rendering
  if (mode == 0) {
    // compute the transformed and projected vertex position (into gl_Position) 
    // compute the vertex color (into col)
    gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0f);
    col = color;
  }
  // mode for smoothened triangles(4)
  if (mode == 1) {
    float smoothenedX = float(p_left.x + p_right.x + p_up.x + p_down.x) / 4.0f;
  	float smoothenedY = float(p_left.y + p_right.y + p_up.y + p_down.y) / 4.0f;
    float smoothenedZ = float(p_left.z + p_right.z + p_up.z + p_down.z) / 4.0f;
  	gl_Position = projectionMatrix * modelViewMatrix * vec4( smoothenedX, smoothenedY, smoothenedZ, 1.0f);
    float eps = 0.00001f; // to guard divisions by zero
    col = max(color, vec4(eps)) / max(position.y, eps) * smoothenedY;
  }

}

