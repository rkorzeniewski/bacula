<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>Client <TMPL_VAR Name></h1>
 </div>
 <div class='bodydiv'>
<form name="client" action='?' method='GET'>
     <input type='hidden' name='client' value=<TMPL_VAR qclient>>
     <table id='id<TMPL_VAR ID>'></table>
	<div class="otherboxtitle">
          Actions &nbsp;
        </div>
        <div class="otherbox">
       <button type="submit" class="bp" name='action' value='client_save' title="__Apply__"> <img src='/bweb/apply.png' alt=''>__Apply__</button>
        </div>

</form>
 </div>

<script type="text/javascript" language="JavaScript">
var header = new Array("__Group Name__", "__Description__", "__Selection__");

var data = new Array();
var radiobox ;

<TMPL_LOOP client_group>
radiobox = document.createElement('INPUT');
radiobox.type  = 'radio';
radiobox.name = 'client_group';
radiobox.value = '<TMPL_VAR client_group_name>';
radiobox.checked = <TMPL_IF here>1<TMPL_ELSE>0</TMPL_IF>;

data.push( 
  new Array( "<TMPL_VAR client_group_name>", 
	     "<TMPL_VAR comment>",
             radiobox
              )
) ; 
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id<TMPL_VAR ID>",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
// natural_compare: false,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 rows_per_page: rows_per_page,
 disable_sorting: new Array(1)
}
);
</script>
