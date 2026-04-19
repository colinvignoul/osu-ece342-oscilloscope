//Declare encoder variables for global scope
//enc_shift
//enc_scale
//enc_trig

void setup() {
  //Initialize encoder variables here
  //enc_shift = pin A
  //enc_scale = pin B
  //enc_trig = pin C

}

void loop() {
  /*
  Assign boolean variable to horizontal/vertical switch
  bool vert_horz = pin D

  check if encoder has been changed

  if enc_shift != pin A
      if enc_shift = pin A + 1 or enc_shift = pin A - 3 (in case of move from 11 to 00)
        if vert_horz
          shift plot right
        else
          shift plot up
        enc_shift = pin A

      else if enc_shift = pin A - 1 or enc_shift = pin A + 3
        if vert_horz
          shift plot left
        else
          shift plot down
        enc_shift = pin A

  Repeat similar process for scaling and trigger encoders, 
  neglecting horizontal/vertical switch for the trigger
  */
}
