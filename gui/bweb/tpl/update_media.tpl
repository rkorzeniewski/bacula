<br/>
<div class='titlediv'>
 <h1 class='newstitle'>__Update media__ <TMPL_VAR volumename></h1>
</div>
<div class='bodydiv'>
  <form name='form1' action="?" method='GET'>
   <table>
    <tr><td>__Volume Name:__</td>
        <td><input type='text' name='media' class='formulaire' value='<TMPL_VAR volumename>' title='__Change this to update an other volume__'>
        </td>
    </tr>
    <tr><td>__Pool:__</td>
        <td><select name='pool' class='formulaire'>
<TMPL_LOOP db_pools>
             <option value='<TMPL_VAR name>'><TMPL_VAR name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>__Status:__</td>
        <td><select name='volstatus' class='formulaire'>
           <option value='Append'>Append</option>
           <option value='Archive'>Archive</option>
           <option value='Disabled'>Disabled</option>
           <option value='Cleaning'>Cleaning</option>
           <option value='Error'>Error</option>
	   <option value='Full'>Full</option>
           <option value='Read-Only'>Read-Only</option>
           <option value='Used'>Used</option>
	   <option value='Recycle'>Recycle</option>
           </select>
        </td>
    </tr>

    <tr><td>__Slot:__</td>
        <td> 
          <input class='formulaire' type='text' 
                 name='slot' value='<TMPL_VAR slot>'>
        </td>
    </tr>

    <tr><td>__InChanger Flag:__</td>
        <td> 
          <input class='formulaire' type='checkbox' 
               name='inchanger' <TMPL_IF inchanger>checked</TMPL_IF>>
        </td>
    </tr>

    <tr><td>__Enabled:__</td>
        <td> <select name='enabled' class='formulaire'>
           <option value='yes'>__yes__</option>
           <option value='no'>__no__</option>
           <option value='archived'>__archived__</option>
           </select>
        </td>
    </tr>

    <tr><td> __Location:__ </td>
        <td><select name='location' class='formulaire'>
        <option value=''></option>
  <TMPL_LOOP db_locations>
      <option value='<TMPL_VAR location>'><TMPL_VAR location></option>
  </TMPL_LOOP>
    </select>
        </td>
    </tr>
    <tr><td> __Retention period:__ </td>
        <td>
          <input class='formulaire' type='text' title='__ex: 3 days, 1 month__'
               name='volretention' value='<TMPL_VAR volretention>'>
        </td>
    </tr>
    <tr><td> __Use duration:__ </td>
        <td>
          <input class='formulaire' type='text' title='__ex: 3 days, 1 month__'
               name='voluseduration' value='<TMPL_VAR voluseduration>'>
        </td>
    </tr>
    <tr><td> __Max Jobs:__ </td>
        <td>
          <input class='formulaire' type='text' title='__ex: 10__'
               name='maxvoljobs' value='<TMPL_VAR maxvoljobs>'>
        </td>
    </tr>
    <tr><td> __Max Files:__ </td>
        <td>
          <input class='formulaire' type='text' title='__ex: 10000__'
               name='maxvolfiles' value='<TMPL_VAR maxvolfiles>'>
        </td>
    </tr>
    <tr><td> __Max Bytes:__ </td>
        <td>
          <input class='formulaire' type='text' title='__ex: 10M, 11G__'
               name='maxvolbytes' value='<TMPL_VAR maxvolbytes>'>
        </td>
    </tr>
    <tr><td>__Recycle Pool:__</td>
        <td><select name='poolrecycle' class='formulaire'>
<TMPL_LOOP db_pools>
             <option value='<TMPL_VAR name>'><TMPL_VAR name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td> __Comment:__ </td>
        <td>
          <input class='formulaire' type='text' title='__a comment__'
               name='comment' value='<TMPL_VAR comment>'>
        </td>
    </tr>

    </table>
<table>
 <td>
  <button type="submit" class="bp" name='action' value='do_update_media'> <img src='/bweb/apply.png' alt=''> __Apply__ </button>
  <button type="submit" class="bp" name='action' title='__Update from pool__'
    value='update_from_pool'> <img src='/bweb/update.png' alt=''> __Update from pool__ </button>
 </form>
 </td>
 <td>
  <form action='?' method='GET'>
   <input type='hidden' name='pool' value='<TMPL_VAR poolname>'>
    <button type="submit" class="bp" name='action' value='media'>
     <img src='/bweb/zoom.png' alt=''>__View Pool__ </button>
  </form>
 </td>
</table>
</div>

<script type="text/javascript" language='JavaScript'>
var ok=1;
for (var i=0; ok && i < document.form1.pool.length; ++i) {
   if (document.form1.pool[i].value == '<TMPL_VAR poolname>') {
      document.form1.pool[i].selected = true;
      ok=0;
   }
}

ok=1;
for (var i=0; ok && i < document.form1.pool.length; ++i) {
   if (document.form1.poolrecycle[i].value == '<TMPL_VAR poolrecycle>') {
      document.form1.poolrecycle[i].selected = true;
      ok=0;
   }
}

ok=1;
for (var i=0; ok && i < document.form1.location.length; ++i) {
   if (document.form1.location[i].value == '<TMPL_VAR location>') {
      document.form1.location[i].selected = true;
      ok=0;
   }
}

ok=1;
for (var i=0; ok && i < document.form1.volstatus.length; ++i) {
   if (document.form1.volstatus[i].value == '<TMPL_VAR volstatus>') {
      document.form1.volstatus[i].selected = true;
      ok=0;
   }
}
ok=1;
for (var i=0; ok && i < document.form1.enabled.length; ++i) {
   if (document.form1.enabled[i].value == '<TMPL_VAR enabled>') {
      document.form1.enabled[i].selected = true;
      ok=0;
   }
}

</script>
