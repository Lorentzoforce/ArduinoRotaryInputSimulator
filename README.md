# ArduinoRotaryInputSimulator
This is a project implementation: the rotation angle of the rotary encoder and potentiometer and other inputs are transmitted to the Windows device,
and the Windows device program simulates and generates mouse and keyboard inputs based on these inputs.
potentiometerRotation.ino is the arduino side code when you connect the potentiometer as your rotating component.
rotary_encoder.ino is the Arduino code when you connect the rotary encoder as your rotary component.

It is worth noting that most modern games (such as HALO) will automatically correct the direction while driving, 
so even with this project, it is difficult to use it to achieve a realistic driving simulation, although this was the original vision of this project.

1. Clone the repository:git clone https://github.com/Lorentzoforce/ArduinoRotaryInputSimulator.git
2. Open the solution file (.sln) in Visual Studio 2022.
3. Compile and run the project.
