
EXTRA_FLAGS=

$(MAKECMDGOALS):
	@$(basename $(MAKEFILE_LIST)).sh -G -j $(EXTRA_FLAGS) $@

.PHONY: $(MAKECMDGOALS)
