
Per the task description, I built the reconstructor project assuming a stream of events processed sequentially, using C++14.

# Thought process
What I had to figure out before starting:
1. What exactly is this project supposed to accomplish? What does it do at runtime, and what's its input/output?
   - Code should take a stream of exchange actions (individual rows, sequentially) and update the output dataset with the state of the orderbook after processing each event.
2. What do each of the input/output columns denote?
   - Input: described in the MBO format [here]( https://databento.com/docs/schemas-and-data-formats/mbo#fields-mbo?historical=cpp&live=cpp&reference=python ).
   - Output: described in the MBP-10 format [here](https://databento.com/docs/schemas-and-data-formats/mbp-10#fields-mbp-10?historical=python&live=python&reference=python).
3. What are the 10 "levels" in the MBP-10 output?
   - Every level (0-9) has columns `bid_px/bid_sz/bid_ct/ask_px/ask_sz/ask_ct`.
   - Each level is just the Nth best price for the bid/ask side, up to 10th best.
4. What project structure would best fit project requirements?
   - Because there's structured input & output formats and a core order book reconstructor in between them, I can neatly subdivide the source code into an MBO "message parser" (reading individual rows), the reconstruction engine, and an MBP writer. 
   - The parser and writer components are I/O-bound, while the engine is CPU-bound. This subdivision helps with testing and targeting performance improvements.