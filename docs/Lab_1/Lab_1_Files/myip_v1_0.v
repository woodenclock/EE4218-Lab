/*
-------------------------------------------------------------------------------
--
-- Definition of Ports
-- ACLK           : Synchronous clock
-- ARESETN        : System reset, active low
-- S_AXIS_TREADY  : Ready to accept data in
-- S_AXIS_TDATA   :  Data in 
-- S_AXIS_TLAST   : Optional data in qualifier
-- S_AXIS_TVALID  : Data in is valid
-- M_AXIS_TVALID  :  Data out is valid
-- M_AXIS_TDATA   : Data Out
-- M_AXIS_TLAST   : Optional data out qualifier
-- M_AXIS_TREADY  : Connected slave device is ready to accept data out
--
-------------------------------------------------------------------------------
*/
`include "sharedparams.vh"

module myip_v1_0 
	(
		// DO NOT EDIT BELOW THIS LINE ////////////////////
		ACLK,
		ARESETN,
		S_AXIS_TREADY,
		S_AXIS_TDATA,
		S_AXIS_TLAST,
		S_AXIS_TVALID,
		M_AXIS_TVALID,
		M_AXIS_TDATA,
		M_AXIS_TLAST,
		M_AXIS_TREADY
		// DO NOT EDIT ABOVE THIS LINE ////////////////////
	);

	input					ACLK;    		     // Synchronous clock
	input					ARESETN; 		     // System reset, active low
	// slave in interface
	output	reg				S_AXIS_TREADY;       // Ready to accept data in
	input	[31 : 0]		S_AXIS_TDATA;        // Data in
	input					S_AXIS_TLAST;        // Optional data in qualifier
	input					S_AXIS_TVALID;       // Data in is valid
	// master out interface
	output	reg				M_AXIS_TVALID;       // Data out is valid
	output	reg [31 : 0]	M_AXIS_TDATA = 0;    // Data Out
	output	reg				M_AXIS_TLAST;        // Optional data out qualifier
	input					M_AXIS_TREADY;       // Connected slave device is ready to accept data out

//----------------------------------------
// Implementation Section
//----------------------------------------
// In this section, we provide an example implementation of MODULE myip_v1_0
// that does the following:
//
// 1. Read all inputs
// 2. Add each input to the contents of register 'sum' which acts as an accumulator
// 3. After all the inputs have been read, write out the content of 'sum', 'sum+1', 'sum+2', 'sum+3'
//
// You will need to modify this example for
// MODULE myip_v1_0 to implement your coprocessor
// RAM parameters for assignment 1    
    localparam A_elements = `M * `N;
	localparam B_elements = `N;
	localparam RES_elements = `M;
	
	localparam A_depth_bits = $clog2(`M * `N );  	    // 8 elements (A is a 2x4 matrix)
	localparam B_depth_bits = $clog2(`N ); 	    // 4 elements (B is a 4x1 matrix)
	localparam RES_depth_bits = $clog2(`M );	// 2 elements (RES is a 2x1 matrix)
	localparam width = `WIDTH;			            // all 8-bit data
	
// wires (or regs) to connect to RAMs and matrix_multiply_0 for assignment 1
// those which are assigned in an always block of myip_v1_0 shoud be changes to reg.
	reg    	A_write_en;									// myip_v1_0 -> A_RAM. To be assigned within myip_v1_0. Possibly reg.
	reg    	[A_depth_bits-1:0] A_write_address;			// myip_v1_0 -> A_RAM. To be assigned within myip_v1_0. Possibly reg. 
	reg    	[width-1:0] A_write_data_in;				// myip_v1_0 -> A_RAM. To be assigned within myip_v1_0. Possibly reg.
	wire    A_read_en;									// matrix_multiply_0 -> A_RAM.
	wire    [A_depth_bits-1:0] A_read_address;		    // matrix_multiply_0 -> A_RAM.
	wire	[width-1:0] A_read_data_out;				// A_RAM -> matrix_multiply_0.
	reg    	B_write_en;									// myip_v1_0 -> B_RAM. To be assigned within myip_v1_0. Possibly reg.
	reg	    [B_depth_bits-1:0] B_write_address;			// myip_v1_0 -> B_RAM. To be assigned within myip_v1_0. Possibly reg.
	reg 	[width-1:0] B_write_data_in;				// myip_v1_0 -> B_RAM. To be assigned within myip_v1_0. Possibly reg.
	wire 	B_read_en;									// matrix_multiply_0 -> B_RAM.
	wire 	[B_depth_bits-1:0] B_read_address;		    // matrix_multiply_0 -> B_RAM.
	wire	[width-1:0] B_read_data_out;				// B_RAM -> matrix_multiply_0.
	wire 	RES_write_en;								// matrix_multiply_0 -> RES_RAM.
	wire 	[RES_depth_bits-1:0] RES_write_address;		// matrix_multiply_0 -> RES_RAM.
	wire 	[width-1:0] RES_write_data_in;				// matrix_multiply_0 -> RES_RAM.
	reg 	RES_read_en = 0;  							// myip_v1_0 -> RES_RAM. To be assigned within myip_v1_0. Possibly reg.
	reg    [RES_depth_bits-1:0] RES_read_address;	// myip_v1_0 -> RES_RAM. To be assigned within myip_v1_0. Possibly reg.
	wire	[width-1:0] RES_read_data_out;				// RES_RAM -> myip_v1_0
	
	// wires (or regs) to connect to matrix_multiply for assignment 1
	reg 	Start; 										// myip_v1_0 -> matrix_multiply_0. To be assigned within myip_v1_0. Possibly reg.
	wire 	Done;										// matrix_multiply_0 -> myip_v1_0. 
			
				
	// Total number of input data.
	localparam NUMBER_OF_INPUT_WORDS  = A_elements + B_elements; // 2**A_depth_bits + 2**B_depth_bits = 12 for assignment 1

	// Total number of output data
	localparam NUMBER_OF_OUTPUT_WORDS = RES_elements; // 2**RES_depth_bits = 2 for assignment 1

	// Define the states of state machine (one hot encoding)
	localparam Idle  = 4'b1000;
	localparam Read_Inputs = 4'b0100;
	localparam Compute = 4'b0010;
	localparam Write_Outputs  = 4'b0001;	
	localparam Read_A = 0;
	localparam Read_B = 1;

	reg [3:0] state;
	reg [31:0] out_word;
    reg out_valid;

	// Counters to store the number inputs read & outputs written.
	// Could be done using the same counter if reads and writes are not overlapped (i.e., no dataflow optimization)
	// Left as separate for ease of debugging
	reg [$clog2(NUMBER_OF_INPUT_WORDS) - 1:0] read_counter;
	reg [$clog2(NUMBER_OF_OUTPUT_WORDS) - 1:0] write_counter;
    reg read_state;

    // CAUTION:
    // The sequence in which data are read in and written out should be
    // consistent with the sequence they are written and read in the driver's hw_acc.c file

	always @(posedge ACLK) 
	begin
	// implemented as a single-always Moore machine
	// a Mealy machine that asserts S_AXIS_TREADY and captures S_AXIS_TDATA etc can save a clock cycle
		/****** Synchronous reset (active low) ******/
		if (!ARESETN)
		begin
			// CAUTION: make sure your reset polarity is consistent with the system reset polarity
			state <= Idle;
        end
		else
		begin
			case (state)
				Idle:
				begin
					read_counter 	<= 0;
					write_counter 	<= 0;
					out_valid       <= 0;
                    out_word        <= 0;
					S_AXIS_TREADY 	<= 0;
					M_AXIS_TVALID 	<= 0;
					M_AXIS_TLAST  	<= 0;
					RES_read_en     <= 0;
                    RES_read_address <= 0;
					if (S_AXIS_TVALID == 1)
					begin
						state       	<= Read_Inputs;
						S_AXIS_TREADY 	<= 1; 
						read_state      <= Read_A;
						// start receiving data once you go into Read_Inputs
					end
				end

				Read_Inputs:
				begin
					S_AXIS_TREADY 	<= 1;
					M_AXIS_TLAST <= 0;
					A_write_en <= 0;
                    B_write_en <= 0;
					read_state <= (read_counter >= A_elements - 1) ? Read_B : Read_A;
					if (S_AXIS_TVALID && S_AXIS_TREADY) 
					begin
					   if (read_state == Read_A)
					   begin
					       A_write_en  <= 1;
					       A_write_address <= read_counter;
					       A_write_data_in <= S_AXIS_TDATA[width-1:0];
					   end
					   else // read_state == Read_B
					   begin
					       A_write_en <= 0;				       
					       B_write_en  <= 1;
					       B_write_address <= read_counter - A_elements;
					       B_write_data_in <= S_AXIS_TDATA[width-1:0];
					   end
						// Coprocessor function (adding the numbers together) happens here (partly)
						// If we are expecting a variable number of words, we should make use of S_AXIS_TLAST.
						// Since the number of words we are expecting is fixed, we simply count and receive 
						// the expected number (NUMBER_OF_INPUT_WORDS) instead.
						if (read_counter == NUMBER_OF_INPUT_WORDS - 1)
						begin
							state      		<= Compute;
							Start           <= 1;
						end
						else
						begin
							read_counter 	<= read_counter + 1;
						end
					end
				end
            
				Compute:
				begin
				    S_AXIS_TREADY <= 0;
				    M_AXIS_TLAST <= 0;
                    read_counter <= 0;
                    A_write_en <= 0;
                    B_write_en <= 0;
                    Start <= 0;  // Deassert Start after 1 cycle
					// Coprocessor function to be implemented (matrix multiply) should be here. Right now, nothing happens here.
					if (Done) begin
						Start 		        <= 0;
						RES_read_en 		<= 1;
						RES_read_address    <= 0;
						out_valid           <= 0;
					end
					else if (RES_read_en) begin
					    state            <= Write_Outputs;
                        write_counter    <= 0;
                        out_valid <= 0;
                        // M_AXIS_TVALID    <= 0;
                        //RES_read_address <= 1;
					end
					// Possible to save a cycle by asserting M_AXIS_TVALID and presenting M_AXIS_TDATA just before going into 
					// Write_Outputs state. However, need to adjust write_counter limits accordingly
					// Alternatively, M_AXIS_TVALID and M_AXIS_TDATA can be asserted combinationally to save a cycle.
				end
				
				Write_Outputs: begin
                        RES_read_en <= 1;                    
                        M_AXIS_TVALID <= out_valid;
                        M_AXIS_TDATA  <= out_word;
                        M_AXIS_TLAST  <= out_valid && (write_counter == NUMBER_OF_OUTPUT_WORDS - 1);
                    
                        if (out_valid && M_AXIS_TREADY) begin
                            if (write_counter == NUMBER_OF_OUTPUT_WORDS - 1) begin
                                state       <= Idle;
                                out_valid   <= 0;
                                RES_read_en <= 0;
                            end else begin
                                write_counter    <= write_counter + 1;
                                RES_read_address <= RES_read_address + 1;
                                out_valid        <= 0; // next cycle we'll capture the new RAM output
                            end
                        end else if (!out_valid) begin
                            // Only capture when we are NOT transferring this cycle
                            out_word  <= {24'b0, RES_read_data_out};
                            out_valid <= 1;
                        end
                    end
			endcase
		end
	end
	   
	// Connection to sub-modules / components for assignment 1
	
	memory_RAM 
	#(
		.width(width), 
		.depth_bits(A_depth_bits)
	) A_RAM 
	(
		.clk(ACLK),
		.write_en(A_write_en),
		.write_address(A_write_address),
		.write_data_in(A_write_data_in),
		.read_en(A_read_en),    
		.read_address(A_read_address),
		.read_data_out(A_read_data_out)
	);
										
										
	memory_RAM 
	#(
		.width(width), 
		.depth_bits(B_depth_bits)
	) B_RAM 
	(
		.clk(ACLK),
		.write_en(B_write_en),
		.write_address(B_write_address),
		.write_data_in(B_write_data_in),
		.read_en(B_read_en),    
		.read_address(B_read_address),
		.read_data_out(B_read_data_out)
	);
										
										
	memory_RAM 
	#(
		.width(width), 
		.depth_bits(RES_depth_bits)
	) RES_RAM 
	(
		.clk(ACLK),
		.write_en(RES_write_en),
		.write_address(RES_write_address),
		.write_data_in(RES_write_data_in),
		.read_en(RES_read_en),    
		.read_address(RES_read_address),
		.read_data_out(RES_read_data_out)
	);
										
	matrix_multiply 
	#(
		.width(width), 
		.A_depth_bits(A_depth_bits), 
		.B_depth_bits(B_depth_bits), 
		.RES_depth_bits(RES_depth_bits) 
	) matrix_multiply_0
	(									
		.clk(ACLK),
		.Start(Start),
		.Done(Done),
		
		.A_read_en(A_read_en),
		.A_read_address(A_read_address),
		.A_read_data_out(A_read_data_out),
		
		.B_read_en(B_read_en),
		.B_read_address(B_read_address),
		.B_read_data_out(B_read_data_out),
		
		.RES_write_en(RES_write_en),
		.RES_write_address(RES_write_address),
		.RES_write_data_in(RES_write_data_in)
	);

endmodule