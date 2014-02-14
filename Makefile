
WAF=./waf
TEE=tee
TIMESTAMP=date +%Y%m%d-%H%M%S

DIR=./formulae
LIMIT=600
JOBS=10
MODE=vwu

all: build

clean:
	$(WAF) clean

distclean:
	$(WAF) distclean
	
dist:
	$(WAF) dist
	
configure:
	$(WAF) configure -j $(JOBS)

build: configure
	$(WAF) build -j $(JOBS)

batch:
	$(WAF) batch -F $(DIR) -L $(LIMIT) -j $(JOBS) -M $(MODE) | $(TEE) batch-$(shell $(TIMESTAMP)).log 

graph:
	$(WAF) graph