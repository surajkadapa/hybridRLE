/*
* generate an image using the PGM(Protable Gray Map) format
* PGM stores pixel values as plain text/binary
* header format is as follows
* -------------------------------
* P5
* width height
* max_gray_value
* -------------------------------
* max_gray_value is usually 255(for an 8-bit grayscale image)
*/
#include <stdio.h>
#include <stdint.h>

#define WIDTH 4096
#define HEIGHT 4096

void generate_image(const char *filename){
  FILE *file = fopen(filename, "wb");
  if(!file){
    printf("[ERROR]cannot open file\n");
    return;
  }
  
  //writing the PGM header
  fprintf(file, "P5\n%d %d\n255\n", WIDTH, HEIGHT);
  
  //generating the image
  for(int y = 0; y < HEIGHT; y++){
    for(int x = 0; x < WIDTH; x++){
      uint8_t pixel = (uint8_t) (x);
      fwrite(&pixel, 1, 1, file);
    }
  }

  fclose(file);
  printf("[MESSAGE]image generated successfull\n");
}

int main(){
  generate_image("image.pgm");
  return 0;
}
