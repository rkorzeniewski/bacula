
all:
	@echo "make [gen-pot|gen-tpl|gen-mo|update-po]"

update: gen-mo gen-tpl

gen-pot:
	../script/make_pot.pl ../tpl/*.tpl

gen-mo: gen-pot update-po
	mkdir -p fr/LC_MESSAGES
	mkdir -p es/LC_MESSAGES
	msgfmt -o fr/LC_MESSAGES/bweb.mo fr.po
	msgfmt -o es/LC_MESSAGES/bweb.mo es.po

update-po: update-po-fr update-po-es

update-po-fr: gen-pot
	msgmerge -U fr.po bweb.pot

update-po-es: gen-pot
	msgmerge -U es.po bweb.pot

gen-tpl: gen-tpl-fr gen-tpl-es gen-tpl-en

gen-tpl-fr:
	(cd .. ; LANGUAGE=fr ./script/tpl_generate.pl ./tpl/*.tpl)

gen-tpl-es:
	(cd .. ; LANGUAGE=es ./script/tpl_generate.pl ./tpl/*.tpl)

gen-tpl-en:
	(cd .. ; LANGUAGE=es ./script/tpl_generate.pl ./tpl/*.tpl)