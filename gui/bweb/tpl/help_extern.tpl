<br/>
<div class='titlediv'>
 <h1 class='newstitle'> Help ejecting media (part 1/2)</h1>
</div>
<div class='bodydiv'>
This tool will select the best candidates to eject. You will
be asked to make your selection on the next screen. 
  <form action="?" method='GET'>
   <table>
    <tr><td>Pool:</td>      
        <td><select name='pool' class='formulaire' multiple>
<TMPL_LOOP NAME=db_pools>
             <option selected><TMPL_VAR NAME=name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>Media Type:</td>      
        <td><select name='mediatype' class='formulaire' multiple>
<TMPL_LOOP NAME=db_mediatypes>
             <option><TMPL_VAR NAME=mediatype></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td> Location : </td>
        <td><select name='location' class='formulaire'>
  <TMPL_LOOP NAME=db_locations>
      <option id='loc_<TMPL_VAR NAME=location>' value='<TMPL_VAR NAME=location>'><TMPL_VAR NAME=location></option>
  </TMPL_LOOP>
    </select>
        </td>
    </tr>
    <tr>
        <td>Number of media <br/> to eject:</td> 
        <td> <input type='text' name='limit' size='3' class='formulaire' 
              value='10'> </td>
    </tr>
    <tr>
        <td><input type="image" name='action' value='compute_extern_media' title='Next' src='/bweb/next.png'>
        </td><td/>
    </tr>
   </table>
   </form>
</div>
