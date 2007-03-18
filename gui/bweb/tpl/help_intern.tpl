<br/>
<div class='titlediv'>
  <h1 class='newstitle'> Help loading media (part 1/2)</h1>
</div>
<div class="bodydiv">
This tool will select the best candidates to load. You will
be asked to make your selection on the next screen. 
  <form action="?" method='GET'>
   <table>
    <tr><td>Pool:</td>      
        <td><select name='pool' class='formulaire' multiple>
<TMPL_LOOP db_pools>
             <option selected><TMPL_VAR name></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>Media Type:</td>      
        <td><select name='mediatype' class='formulaire' multiple>
<TMPL_LOOP db_mediatypes>
             <option><TMPL_VAR mediatype></option>
</TMPL_LOOP>
           </select>
        </td>
    </tr>
    <tr><td>
    Location : 
    </td><td><select name='location' class='formulaire'>
  <TMPL_LOOP db_locations>
      <option value='<TMPL_VAR location>'><TMPL_VAR location></option>
  </TMPL_LOOP>
    </select>
    </td>
    </tr>
    <tr>
        <td>Expired :</td> 
        <td> <input type='checkbox' name='expired' class='formulaire' 
                checked> </td>
    </tr>
    <tr>
        <td>Number of media <br/> to load:</td> 
        <td> <input type='text' name='limit' class='formulaire' 
                size='3' value='10'> </td>
    </tr>
    <tr>
        <td><input type="image" name='action' value='compute_intern_media' 
                title='Next' src='/bweb/next.png'>
        </td><td/>
    </tr>
   </table>
   </form>
</div>
