# ArduinoRotaryInputSimulator
This is a project implementation: the rotation angle of the rotary encoder and potentiometer and other inputs are transmitted to the Windows device,
and the Windows device program simulates and generates mouse and keyboard inputs based on these inputs.
potentiometerRotation.ino is the arduino side code when you connect the potentiometer as your rotating component.
rotary_encoder.ino is the Arduino code when you connect the rotary encoder as your rotary component.

It's worth noting that most modern games (like HALO) automatically return to the correct direction, 
so even with this project, it's difficult to achieve a realistic driving simulation, even though that was the original vision of the project.
