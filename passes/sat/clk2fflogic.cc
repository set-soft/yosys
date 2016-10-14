/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Clifford Wolf <clifford@clifford.at>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "kernel/yosys.h"
#include "kernel/sigtools.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct Clk2fflogicPass : public Pass {
	Clk2fflogicPass() : Pass("clk2fflogic", "convert clocked FFs to generic $ff cells") { }
	virtual void help()
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    clk2fflogic [options] [selection]\n");
		log("\n");
		log("This command replaces clocked flip-flops with generic $ff cells that use the\n");
		log("implicit global clock. This is useful for formal verification of designs with\n");
		log("multiple clocks.\n");
		log("\n");
	}
	virtual void execute(std::vector<std::string> args, RTLIL::Design *design)
	{
		// bool flag_noinit = false;

		log_header(design, "Executing CLK2FFLOGIC pass (convert clocked FFs to generic $ff cells).\n");

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++)
		{
			// if (args[argidx] == "-noinit") {
			// 	flag_noinit = true;
			// 	continue;
			// }
			break;
		}
		extra_args(args, argidx, design);

		for (auto module : design->selected_modules())
		{
			SigMap sigmap(module);
			dict<SigBit, State> initbits;
			pool<SigBit> del_initbits;
			vector<Cell*> ffcells;

			for (auto wire : module->wires())
				if (wire->attributes.count("\\init") > 0)
				{
					Const initval = wire->attributes.at("\\init");
					SigSpec initsig = sigmap(wire);

					for (int i = 0; i < GetSize(initval) && i < GetSize(initsig); i++)
						if (initval[i] == State::S0 || initval[i] == State::S1)
							initbits[initsig[i]] = initval[i];
				}

			for (auto cell : module->selected_cells())
				if (cell->type.in("$dff"))
					ffcells.push_back(cell);

			for (auto cell : ffcells)
			{
				if (cell->type == "$dff")
				{
					bool clkpol = cell->parameters["\\CLK_POLARITY"].as_bool();

					SigSpec clk = cell->getPort("\\CLK");
					SigSpec past_clk = module->addWire(NEW_ID);
					module->addFf(NEW_ID, clk, past_clk);

					SigSpec sig_d = cell->getPort("\\D");
					SigSpec sig_q = cell->getPort("\\Q");

					log("Replacing %s.%s (%s): CLK=%s, D=%s, Q=%s\n",
							log_id(module), log_id(cell), log_id(cell->type),
							log_signal(clk), log_signal(sig_d), log_signal(sig_q));
					module->remove(cell);

					SigSpec clock_edge = module->Eqx(NEW_ID, {past_clk, clk},
							clkpol ? SigSpec({State::S0, State::S1}) : SigSpec({State::S1, State::S0}));

					Wire *past_d = module->addWire(NEW_ID, GetSize(sig_d));
					Wire *past_q = module->addWire(NEW_ID, GetSize(sig_q));
					module->addFf(NEW_ID, sig_d, past_d);
					module->addFf(NEW_ID, sig_q, past_q);

					module->addMux(NEW_ID, past_q, past_d, clock_edge, sig_q);

					Const initval;
					bool assign_initval = false;
					for (int i = 0; i < GetSize(sig_d); i++) {
						SigBit qbit = sigmap(sig_q[i]);
						if (initbits.count(qbit)) {
							initval.bits.push_back(initbits.at(qbit));
							del_initbits.insert(qbit);
						} else
							initval.bits.push_back(State::Sx);
						if (initval.bits.back() != State::Sx)
							assign_initval = true;
					}

					if (assign_initval) {
						past_d->attributes["\\init"] = initval;
						past_q->attributes["\\init"] = initval;
					}

					continue;
				}

				log_abort();
			}

			for (auto wire : module->wires())
				if (wire->attributes.count("\\init") > 0)
				{
					bool delete_initattr = true;
					Const initval = wire->attributes.at("\\init");
					SigSpec initsig = sigmap(wire);

					for (int i = 0; i < GetSize(initval) && i < GetSize(initsig); i++)
						if (del_initbits.count(initsig[i]) > 0)
							initval[i] = State::Sx;
						else if (initval[i] != State::Sx)
							delete_initattr = false;

					if (delete_initattr)
						wire->attributes.erase("\\init");
					else
						wire->attributes.at("\\init") = initval;
				}
		}

	}
} Clk2fflogicPass;

PRIVATE_NAMESPACE_END
