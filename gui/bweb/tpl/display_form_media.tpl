<div class="otherboxtitle">
  Filter &nbsp;
</div>
<div class="otherbox">
<form name='form1' action='?' method='GET'>
<table border='0'>
<tr>
  <td valign='top'>
    <h2>Media Type</h2>
    <select name='mediatype' class='formulaire'>
      <option id='mediatype_all' value=''></option>
<TMPL_LOOP NAME=db_mediatypes>
      <option id='mediatype_<TMPL_VAR mediatype>'><TMPL_VAR mediatype></option>
</TMPL_LOOP>
    </select>     
  </td>
</tr>
<tr>
  <td valign='top'>
    <h2>Location</h2>
    <select name='location' class='formulaire'>
      <option id='location_all>' value=''></option>
<TMPL_LOOP NAME=db_locations>
      <option id='location_<TMPL_VAR location>'><TMPL_VAR location></option>
</TMPL_LOOP>
    </select>     
  </td>
</tr>
<tr>
 <td valign='top'>
    <h2>Status</h2>
    <select name='volstatus' class='formulaire'>
      <option id='volstatus_All' value=''></option>
      <option id='volstatus_Append' value='Append'>Append</option>
      <option id='volstatus_Full'   value='Full'>Full</option>
      <option id='volstatus_Error'  value='Error'>Error</option>
      <option id='volstatus_Used'   value='Used'>Used</option>
      <option id='volstatus_Purged' value='Purged'>Purged</option>
      <option id='volstatus_Recycle' value='Recycle'>Recycle</option>
    </select>     
  </td>
</tr>
<tr>
 <td valign='top'>
    <h2>Pool</h2>
    <select name='pool' class='formulaire'>
      <option id='pool_all>' value=''></option>
<TMPL_LOOP NAME=db_pools>
      <option id='pool_<TMPL_VAR name>'><TMPL_VAR name></option>
</TMPL_LOOP>
    </select>     
  </td>
</tr>
 <tr>
  <td valign='bottom'> 
    <h2>Name</h2>
    <input type='text' name='re_media' 
      <TMPL_IF qre_media>value=<TMPL_VAR qre_media></TMPL_IF>
	class='formulaire' size='8'>
  </td>
</tr>
 <tr>
  <td valign='bottom'> 
    <h2>Expired media</h2>
    <input type='checkbox' name='expired' <TMPL_IF expired> checked </TMPL_IF> 
	class='formulaire'>
  </td>
</tr>
</tr>
 <tr>
  <td valign='bottom'> 
    <h2>Number of items</h2>
    <input type='text' name='limit' value='<TMPL_VAR limit>' 
	class='formulaire' size='4'>
  </td>
</tr>

</table>
  <input type="image" name='action' value='media' src='/bweb/update.png'>

</form>
</div>
<script type="text/javascript" language="JavaScript">
  <TMPL_IF volstatus>
     document.getElementById('volstatus_<TMPL_VAR volstatus>').selected=true;
  </TMPL_IF>

  <TMPL_LOOP qmediatypes>
     document.getElementById('mediatype_' + <TMPL_VAR name>).selected = true;
  </TMPL_LOOP>

  <TMPL_LOOP qpools>
     document.getElementById('pool_' + <TMPL_VAR name>).selected = true;
  </TMPL_LOOP>
  <TMPL_LOOP qlocations>
     document.getElementById('location_' + <TMPL_VAR name>).selected = true;
  </TMPL_LOOP>

</script>

