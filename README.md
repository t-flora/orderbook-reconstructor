# Order book reconstructor
I built the reconstructor project assuming a stream of events processed sequentially, using C++14. It computes an MBP-10 order book from an MBO dataset, and is divided into 3 main components: MBO parser, order book reconstruction engine, and MBP writer. I drew heavy inspiration from the Databento example to understand the functioning of the order book class.

## Optimization decisions
- Using `string_view` to avoid heap allocations & memory copying during parsing
- Using `final` classes since they have no derived classes
- Using `constexpr` where possible

# MBP output rules
The output matches `data/mbp.csv` exactly (`make test`). An MBO event is written to the MBP output when:
- It's an Add or Cancel at depth 0-10 (depth is computed before the event is applied), even if the top 10 levels don't change
- It's a bid-side Cancel at depth 11 that removes its price level
- It's a Clear, or a Trade with side `N` (which doesn't alter the book)
- It completes a T->F->C sequence, which is written as a single Trade on the side of the resting order, using the Trade's fields

# Desired improvements
- Faster parsing of input values and faster output writing using fixed size char buffers, and using buffers directly
- Using memory-mapped files for the MBO parser, eliminating string conversions for fast integer parsing
- Avoiding double conversion for price parsing - parsing directly to fixed-point
- Pre-formatted caches for values like prices to avoid formatting multiple times
- Test alternative containers for order book data structures, e.g. sorted vectors instead of `std::map` if for small vector sizes it's worth it
- Tracking order book state changes incrementally
- Defining a Dockerfile for reproducibility in different environments

# Improvements?
- Robustness to out-of-order messages. The project assumes MBO messages arrive sequentially, with timestamps always increasing as messages arrive. It's unclear to me how meaningful an improvement it would be to be robust to out-of-order messages - effectively rewriting MBP output? - so I'd be very curious to learn if it is.
- Templates? If there's a world where more general implementations make sense, a header-only version of the source code could become worth it.

# Thought process
What I had to figure out before starting:
1. What exactly is this project supposed to accomplish? What does it do at runtime, and what's its input/output?
   - Code should take a stream of exchange actions (individual rows, sequentially) and update the output dataset with the state of the order book after processing each event.
2. What do each of the input/output columns denote?
   - Input: described in the MBO format [here]( https://databento.com/docs/schemas-and-data-formats/mbo#fields-mbo?historical=cpp&live=cpp&reference=python ).
   - Output: described in the MBP-10 format [here](https://databento.com/docs/schemas-and-data-formats/mbp-10#fields-mbp-10?historical=python&live=python&reference=python).
3. What are the 10 "levels" in the MBP-10 output?
   - Every level (0-9) has columns `bid_px/bid_sz/bid_ct/ask_px/ask_sz/ask_ct`.
   - Each level is just the Nth best price for the bid/ask side, up to 10th best.
4. What project structure would best fit project requirements?
   - Because there's structured input & output formats and a core order book reconstructor in between them, I can neatly subdivide the source code into an MBO "message parser" (reading individual rows), the reconstruction engine, and an MBP writer. 
   - The parser and writer components are I/O-bound, while the engine is CPU-bound. This subdivision helps with testing and targeting performance improvements.
5. What can I optimize for this particular sample data?
   - Many data fields in the input are just copied to the output file, with no processing. All those fields can be treated as string views to avoid type conversions.
   - Processing a line at a time, it'd be great to use a stack allocation for the buffer holding each line. Running `awk '{print length($0)}' data/sample_dataset.csv | sort -n | tail -1` (on both input/output data) shows how small one can make the char buffer for holding lines in the sample. Going from type specifications, I computed the maximum char length of each field: input buffers for MBO messages can be much smaller since they don't hold the extra 60 fields for bid/ask level data, and have max size of 154 bytes. For cache segment alignment and safety, I set the buffer to 256 bytes (power of two, product of 64)
6. How should I handle each event in the order book logic?
   - Running `grep -E "^[0-9]" data/mbo.csv | cut -d',' -f6 | sort | uniq -c` shows there are 5 possible actions, including the 1 clear order book action at the beginning, 11 Trade actions (don't affect order book) and 11 Fill actions (don't affect order book)
   - It took a while to figure out what 'depth=11' updates were, but I realized they were cancel orders for orders that were pushed out of the top 10 levels of the orderbook.